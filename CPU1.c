//###########################################################################
//
// FILE:   adc_soc_epwm_cpu01.c
//
// TITLE:  ADC triggering via epwm for F2837xD.
//
//! \addtogroup cpu01_example_list
//! <h1> ADC ePWM Triggering (adc_soc_epwm)</h1>
//!
//! This example sets up the ePWM to periodically trigger the ADC.
//!
//! After the program runs, the memory will contain:\n
//! - \b AdcaResults \b: A sequence of analog-to-digital conversion samples from
//! pin A0. The time between samples is determined based on the period
//! of the ePWM timer.
//
//###########################################################################
// $TI Release: F2837xD Support Library v210 $
// $Release Date: Tue Nov  1 14:46:15 CDT 2016 $
// $Copyright: Copyright (C) 2013-2016 Texas Instruments Incorporated -
//             http://www.ti.com/ ALL RIGHTS RESERVED $
//###########################################################################

//
// Included Files
//
#include <math.h>
#include "F28x_Project.h"

//
// Function Prototypes
//
void ConfigureADC(void);
void ConfigureEPWM(void);
void SetupADCEpwm(Uint16 channelA, Uint16 channelB);
interrupt void epwm1_isr(void);
interrupt void adca1_isr(void);
void InitEPwm();

//
// Defines
//
#define RESULTS_BUFFER_SIZE 	 100U
#define EX_ADC_RESOLUTION         16U

#define EPWM_TIMER_TBPRD  		1000U
#define EPWM_MAX_CMP      		 950U
#define EPWM_MIN_CMP       		  50U

#define EPWM_CMP_UP         	   1U
#define EPWM_CMP_DOWN        	   0U

//
// Globals
//

Uint16 AdcaResults[RESULTS_BUFFER_SIZE];	//Capacitor Voltage Readings
Uint16 AdcbResults[RESULTS_BUFFER_SIZE];	//Load-Side Current Readings
Uint16 resultsIndex;
volatile Uint16 bufferFull;

//PWM Signals
int16 reference;
Uint16 theta, delTheta, compareValue, frequency, modIndex;

void main(void)
{
//
// Step 1. Initialize System Control:
// PLL, WatchDog, enable Peripheral Clocks
// This example function is found in the F2837xD_SysCtrl.c file.
//
    InitSysCtrl();

//
// Step 2. Initialize GPIO:
// This example function is found in the F2837xD_Gpio.c file and
// illustrates how to set the GPIO to it's default state.
//
    InitGpio(); // Skipped for this example

//
// Step 3. Clear all interrupts and initialize PIE vector table:
// Disable CPU interrupts
//
    DINT;

//
// Initialize the PIE control registers to their default state.
// The default state is all PIE interrupts disabled and flags
// are cleared.
// This function is found in the F2837xD_PieCtrl.c file.
//
    InitPieCtrl();

//
// Disable CPU interrupts and clear all CPU interrupt flags:
//
    IER = 0x0000;
    IFR = 0x0000;

//
// Initialize the PIE vector table with pointers to the shell Interrupt
// Service Routines (ISR).
// This will populate the entire table, even if the interrupt
// is not used in this example.  This is useful for debug purposes.
// The shell ISR routines are found in F2837xD_DefaultIsr.c.
// This function is found in F2837xD_PieVect.c.
//
    InitPieVectTable();

//
// Map ISR functions
//
    EALLOW;
    PieVectTable.EPWM1_INT = &epwm1_isr;
    PieVectTable.ADCA1_INT = &adca1_isr; //function for ADCA interrupt 1
    EDIS;

//
// Initialize EPWM
//
    InitEPwm();

//
// Configure the ADC and power it up
//
    ConfigureADC();

//
// Configure the ePWM
//
    ConfigureEPWM();

//
// Setup the ADC for ePWM triggered conversions on channels 0 and 2
//
    SetupADCEpwm(0, 2);

//
// Enable global Interrupts and higher priority real-time debug events:
//
    IER |= (M_INT1 & M_INT3); //Enable group 1 and 3 interrupts
    EINT;  // Enable Global interrupt INTM
    ERTM;  // Enable Global realtime interrupt DBGM

//
// Initialize results buffer
//
    for(resultsIndex = 0; resultsIndex < RESULTS_BUFFER_SIZE; resultsIndex++)
    {
        AdcaResults[resultsIndex] = 0;
        AdcbResults[resultsIndex] = 0;
    }
    resultsIndex = 0;
    bufferFull = 0;

//
// enable PIE interrupt
//
    PieCtrlRegs.PIEIER1.bit.INTx1 = 1;

//
// sync ePWM
//
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;

//
//take conversions indefinitely in loop
//
    do
    {
        //
        //wait while ePWM causes ADC conversions, which then cause interrupts,
        //which fill the results buffer, eventually setting the bufferFull
        //flag
        //
        while(!bufferFull);

        //TODO: Add SPI/DMA Code

        bufferFull = 0; //clear the buffer full flag

    }while(1);
}

//
// ConfigureADC - Write ADC configurations and power up the ADC for both
//                ADC A and ADC B
//
void ConfigureADC(void)
{
    EALLOW;

    //
    //write configurations
    //
    AdcaRegs.ADCCTL2.bit.PRESCALE = 6; //set ADCCLK divider to /4
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_16BIT, ADC_SIGNALMODE_DIFFERENTIAL);

    AdcbRegs.ADCCTL2.bit.PRESCALE = 6; //set ADCCLK divider to /4
	AdcSetMode(ADC_ADCB, ADC_RESOLUTION_16BIT, ADC_SIGNALMODE_DIFFERENTIAL);

    //
    //Set pulse positions to late
    //
    AdcaRegs.ADCCTL1.bit.INTPULSEPOS = 1;
    AdcbRegs.ADCCTL1.bit.INTPULSEPOS = 1;

    //
    //power up the ADC
    //
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;
    AdcbRegs.ADCCTL1.bit.ADCPWDNZ = 1;

    //
    //delay for 1ms to allow ADC time to power up
    //
    DELAY_US(1000);

    EDIS;
}

//
// ConfigureEPWM - Configure EPWM SOC and compare values
//
void ConfigureEPWM(void)
{
    EALLOW;
    // Assumes ePWM clock is already enabled
    EPwm1Regs.ETSEL.bit.SOCAEN    = 0;    // Disable SOC on A group
    EPwm1Regs.ETSEL.bit.SOCASEL    = 4;   // Select SOC on up-count
    EPwm1Regs.ETPS.bit.SOCAPRD = 1;       // Generate pulse on 1st event
    EDIS;
}

//
// SetupADCEpwm - Setup ADC EPWM acquisition window
//
void SetupADCEpwm(Uint16 channelA, Uint16 channelB)
{
    Uint16 acqps;

    //
    //determine minimum acquisition window (in SYSCLKS) based on resolution
    //
    if(ADC_RESOLUTION_12BIT == AdcaRegs.ADCCTL2.bit.RESOLUTION)
    {
        acqps = 14; //75ns
    }
    else //resolution is 16-bit
    {
        acqps = 63; //320ns
    }

    //
    //Select the channels to convert and end of conversion flag
    //
    EALLOW;

    //Channel A - Capacitor Voltage
    AdcaRegs.ADCSOC0CTL.bit.CHSEL = channelA;
    AdcaRegs.ADCSOC0CTL.bit.ACQPS = acqps; //sample window is 100 SYSCLK cycles
    AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 5; //trigger on ePWM1 SOCA/C
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0; //end of SOC0 will set INT1 flag
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;   //enable INT1 flag
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; //make sure INT1 flag is cleared

    //Channel B - Load-Side Current
    AdcbRegs.ADCSOC0CTL.bit.CHSEL = channelB;
    AdcbRegs.ADCSOC0CTL.bit.ACQPS = acqps; //sample window is 100 SYSCLK cycles
	AdcbRegs.ADCSOC0CTL.bit.TRIGSEL = 5; //trigger on ePWM1 SOCA/C
	AdcbRegs.ADCINTSEL1N2.bit.INT1SEL = 0; //end of SOC0 will set INT1 flag
	AdcbRegs.ADCINTSEL1N2.bit.INT1E = 1;   //enable INT1 flag
	AdcbRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; //make sure INT1 flag is cleared
    EDIS;
}

interrupt void epwm1_isr(void)
{
    //
    // Update the CMPA and CMPB values using SPWM modulation
    //
	delTheta = ((0xFFFF)*4*3.1415927*frequency*EPWM_TIMER_TBPRD*(0.00000001));
	theta = theta + delTheta;
	reference = (int16)modIndex * cos((float)theta/65535);
	compareValue = reference + EPWM_TIMER_TBPRD/2;
	EPwm1Regs.CMPA.bit.CMPA = compareValue;

    //
    // Clear INT flag for this timer
    //
    EPwm1Regs.ETCLR.bit.INT = 1;

    //
    // Acknowledge this interrupt to receive more interrupts from group 3
    //
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

//
// adca1_isr - Read ADC Buffer in ISR
//
interrupt void adca1_isr(void)
{
	//Only one interrupt should be necessary- don't need both ADCs to signal when done
    AdcaResults[resultsIndex] = AdcaResultRegs.ADCRESULT0;
    AdcbResults[resultsIndex++] = AdcbResultRegs.ADCRESULT0;
    if(RESULTS_BUFFER_SIZE <= resultsIndex)
    {
        resultsIndex = 0;
        bufferFull = 1;
    }

    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; //clear INT1 flag
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

void InitEPwm()
{
    //
    // Setup TBCLK
    //
    EPwm1Regs.TBPRD = EPWM_TIMER_TBPRD;
    EPwm1Regs.TBPHS.bit.TBPHS = 0x0000;        // Phase is 0
    EPwm1Regs.TBCTR = 0x0000;                  // Clear counter

    EPwm2Regs.TBPRD = EPWM_TIMER_TBPRD;
    EPwm2Regs.TBPHS.bit.TBPHS = 0x0000;        // Phase is 0
    EPwm2Regs.TBCTR = 0x0000;                  // Clear counter

    //
    // Set Compare values
    //
    EPwm1Regs.CMPA.bit.CMPA = EPWM_MIN_CMP;    // Set compare A value
    EPwm1Regs.CMPB.bit.CMPB = 0;    // Set Compare B value

    //EPwm2Regs.CMPA.bit.CMPA = EPWM_MIN_CMP;    // Set compare A value
    //EPwm2Regs.CMPB.bit.CMPB = 0;    // Set Compare B value

    //
    // Setup counter mode
    //
    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up and down
    EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;        // Disable phase loading
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;       // Clock ratio to SYSCLKOUT
    EPwm1Regs.TBCTL.bit.CLKDIV = TB_DIV1;
    EPwm1Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_IN;

    EPwm2Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up and down
	EPwm2Regs.TBCTL.bit.PHSEN = TB_ENABLE;
	EPwm2Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;       // Clock ratio to SYSCLKOUT
	EPwm2Regs.TBCTL.bit.CLKDIV = TB_DIV1;
	EPwm2Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_IN;

    //
    // Setup shadowing
    //
    EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    EPwm1Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO; // Load on Zero
    EPwm1Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    EPwm2Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    EPwm2Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    EPwm2Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO; // Load on Zero
    EPwm2Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    //Setup Deadband - 300 ns
    EPwm1Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
    EPwm1Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;
    EPwm1Regs.DBRED.bit.DBRED = 30;				//Corresponds to 300 ns

    EPwm2Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
    EPwm2Regs.DBCTL.bit.POLSEL = DB_ACTV_HI;
    EPwm2Regs.DBRED.bit.DBRED = 30;				//Corresponds to 300 ns

    //
    // Set actions
    //
    EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;            // Clear PWM1A on event A, up
                                                  // count
    EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;          //Set PWM1A on event A,
                                                  // down count

    EPwm1Regs.AQCTLB.bit.CAU = AQ_CLEAR;
    EPwm1Regs.AQCTLB.bit.CAD = AQ_SET;

    //
    // Interrupt where we will change the Compare Values
    //
    EPwm1Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;     // Select INT on Zero event
    EPwm1Regs.ETSEL.bit.INTEN = 1;                // Enable INT
    EPwm1Regs.ETPS.bit.INTPRD = ET_1ST;           // Generate INT on 1st event
}

//
// End of file
//
