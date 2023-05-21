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
  int DDCupper;

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
      printf("high priority packet received\n");
      if(SDRIP2 == 0 && *(uint32_t *)&addr_from.sin_addr.s_addr != SDRIP)
        continue; // stray msg from inactive client

      DDCupper = (*(uint32_t *)&addr_from.sin_addr.s_addr == SDRIP2);
      if (DDCupper)
        NewMessageReceived2 = true;
      else
      {
        NewMessageReceived = true;
        Byte = (uint8_t)(UDPInBuffer[4]);
        RunBit = (bool)(Byte&1);
        if(RunBit)
        {
          StartBitReceived = true;
          if(ReplyAddressSet && StartBitReceived)
            SDRActive = true;                                       // only set active if we have replay address too
        }
        else
        {
          SDRActive = false;                                       // set state of whole app
          SDRIP = 0;
          printf("set to inactive by client app\n");
          StartBitReceived = false;
        }
      }

//
// now properly decode DDC frequencies
//
      int lower = (DDCupper)?5:0;
      int upper = (DDCupper)?VNUMDDC:5;
      for(i=lower; i<upper; i++)
      {
        LongWord = ntohl(*(uint32_t *)(UDPInBuffer+(i-lower)*4+9));
        SetDDCFrequency(i, LongWord, true);                   // temporarily set above
      }

      if(DDCupper) continue; // skip below for 2nd client

      IsTXMode = (bool)(Byte&2);
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
//      SetSpkrMute((bool)((Byte>>1)&1));
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
      //
      Byte2 = (uint8_t)(UDPInBuffer[1442]);     // RX2 atten
      Byte = (uint8_t)(UDPInBuffer[1443]);      // RX1 atten
      SetADCAttenuator(eADC1, Byte, true, true);
      SetADCAttenuator(eADC2, Byte2, true, true);
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



