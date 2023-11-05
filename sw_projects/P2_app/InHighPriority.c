/////////////////////////////////////////////////////////////
//
// Saturn project: Artix7 FPGA + Raspberry Pi4 Compute Module
// PCI Express interface from linux on Raspberry pi
// this application uses C code to emulate HPSDR protocol 2 
//
// copyright Laurence Barker November 2021
// licenced under GNU GPL3
//
// InHighPriority.c:
//
// handle "incoming high priority" message
//
//////////////////////////////////////////////////////////////

#include "threaddata.h"
#include <stdint.h>
#include "../common/saturntypes.h"
#include "InHighPriority.h"
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "../common/saturnregisters.h"
#include "../common/hwaccess.h"                   // low level access



//
// listener thread for incoming high priority packets
//
void *IncomingHighPriority(void *arg)                   // listener thread
{
  struct ThreadSocketData *ThreadData;                  // socket etc data for this thread
  struct sockaddr_in addr_from;                         // holds MAC address of source of incoming messages
  uint8_t UDPInBuffer[VHIGHPRIOTIYTOSDRSIZE];           // incoming buffer
  struct iovec iovecinst;                               // iovcnt buffer - 1 for each outgoing buffer
  struct msghdr datagram;                               // multiple incoming message header
  int size;                                             // UDP datagram length
  bool RunBit;                                          // true if "run" bit set
  uint32_t DDCPhaseIncrement;                           // delta phase for a DDC
  uint8_t Byte, Byte2;                                  // received dat being decoded
  uint32_t LongWord;
  uint16_t Word;
  int i;                                                // counter
  int Client2;

  extern int TXActive;
  extern uint32_t SDRIP, SDRIP2;

  ThreadData = (struct ThreadSocketData *)arg;
  ThreadData->Active = true;
  printf("spinning up high priority incoming thread with port %d\n", ThreadData->Portid);

  //
  // main processing loop
  //
  while(1)
  {
    memset(&iovecinst, 0, sizeof(struct iovec));
    memset(&datagram, 0, sizeof(datagram));
    iovecinst.iov_base = &UDPInBuffer;                  // set buffer for incoming message number i
    iovecinst.iov_len = VHIGHPRIOTIYTOSDRSIZE;
    datagram.msg_iov = &iovecinst;
    datagram.msg_iovlen = 1;
    datagram.msg_name = &addr_from;
    datagram.msg_namelen = sizeof(addr_from);
    size = recvmsg(ThreadData->Socketid, &datagram, 0);         // get one message. If it times out, ges size=-1
    if(size < 0 && errno != EAGAIN)
    {
      perror("recvfrom, high priority");
      printf("error number = %d\n", errno);
      return NULL;
    }
    //
    // if correct packet, process it
    //
    if(size == VHIGHPRIOTIYTOSDRSIZE)
    {
      //printf("high priority packet received, Client2:%d TXActive:%d\n", Client2, TXActive);
      if(SDRIP2 == 0 && *(uint32_t *)&addr_from.sin_addr.s_addr != SDRIP)
        continue; // stray msg from inactive client

      Client2 = (*(uint32_t *)&addr_from.sin_addr.s_addr == SDRIP2);
//
// now properly decode DDC frequencies
//
      int limit = (Client2)?6:4;
      int offset = (Client2)?0:6;
      for(i=0; i<limit; i++)
      {
        LongWord = ntohl(*(uint32_t *)(UDPInBuffer+i*4+9));
        SetDDCFrequency(i+offset, LongWord, true);                   // temporarily set above
      }

      Byte = (uint8_t)(UDPInBuffer[4]);
      RunBit = (bool)(Byte&1);
      IsTXMode = (bool)(Byte&2);

      if (Client2)
      {
        NewMessageReceived2 = true;

        // just continue for now until TX issues are resolved
        continue;

        if(TXActive == 1) continue;
        TXActive = (IsTXMode)?2:0;
      }
      else
      {
        NewMessageReceived = true;
        if(RunBit)
        {
          StartBitReceived = true;
          if(ReplyAddressSet && StartBitReceived)
          {
            SDRActive = true;                                       // only set active if we have replay address too
            SetTXEnable(true);
          }
        }
        else
        {
          SDRActive = false;                                       // set state of whole app
          SetTXEnable(false);
          EnableCW(false, false);
          //SDRIP = 0;
          printf("set to inactive by client app\n");
          StartBitReceived = false;
        }
        if(TXActive == 2) continue;
        TXActive = (IsTXMode)?1:0;
      }

      SetMOX(IsTXMode);

      //
      // DUC frequency & drive level
      //
      LongWord = ntohl(*(uint32_t *)(UDPInBuffer+329));
      SetDUCFrequency(LongWord, true);
      Byte = (uint8_t)(UDPInBuffer[345]);
      SetTXDriveLevel(Byte);
      //
      // transverter, speaker mute, open collector, user outputs
      //
      Byte = (uint8_t)(UDPInBuffer[1400]);
      SetXvtrEnable((bool)(Byte&1));
      SetSpkrMute((bool)((Byte>>1)&1));
      Byte = (uint8_t)(UDPInBuffer[1401]);
      SetOpenCollectorOutputs(Byte);
      Byte = (uint8_t)(UDPInBuffer[1402]);
      SetUserOutputBits(Byte);
      //
      // Alex
      //
      Word = ntohs(*(uint16_t *)(UDPInBuffer+1430));
      AlexManualRXFilters(Word, 2);
      Word = ntohs(*(uint16_t *)(UDPInBuffer+1432));
      AlexManualTXFilters(Word);
      Word = ntohs(*(uint16_t *)(UDPInBuffer+1434));
      AlexManualRXFilters(Word, 0);
      //
      // RX atten during TX and RX
      // this should be just on RX now, because TX settings are in the DUC specific packet bytes 58&59
      //
      Byte2 = (uint8_t)(UDPInBuffer[1442]);     // RX2 atten
      Byte = (uint8_t)(UDPInBuffer[1443]);      // RX1 atten
      SetADCAttenuator(eADC1, Byte, true, false);
      SetADCAttenuator(eADC2, Byte2, true, false);
      //
      // CWX bits
      //
      Byte = (uint8_t)(UDPInBuffer[5]);      // CWX
      SetCWXBits((bool)(Byte & 1), (bool)((Byte>>2) & 1), (bool)((Byte>>1) & 1));    // enabled, dash, dot
    }
  }
//
// close down thread
//
  close(ThreadData->Socketid);                  // close incoming data socket
  ThreadData->Socketid = 0;
  ThreadData->Active = false;                   // indicate it is closed
  return NULL;
}



