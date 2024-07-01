/////////////////////////////////////////////////////////////
//
// Saturn project: Artix7 FPGA + Raspberry Pi4 Compute Module
// PCI Express interface from linux on Raspberry pi
// this application uses C code to emulate HPSDR protocol 2 
//
// copyright Laurence Barker November 2021
// licenced under GNU GPL3
//
// g2v2panel.h:
//
// interface G2V2 front panel using I2C
//
//////////////////////////////////////////////////////////////

#ifndef __G2V2PANEL_h
#define __G2V2PANEL_h


//
// function to initialise a connection to the G2 V2 front panel; call if selected as a command line option
//
void InitialiseG2V2PanelHandler(void);


//
// function to shutdown a connection to the G2 front panel; call if selected as a command line option
//
void ShutdownG2V2PanelHandler(void);




#endif      // file sentry