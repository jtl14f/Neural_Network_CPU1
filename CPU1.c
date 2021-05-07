//
// Included Files
//
#include "F28x_Project.h"
#include "SineTable.h"

//
// Function Prototypes
//
void ConfigureADC(void);
//void ConfigureEPWM(void);
void SetupADC(Uint16 channelA, Uint16 channelB);
__interrupt void epwm1_isr(void);
__interrupt void adca1_isr(void);
void InitEPwm();
void InitSpi(void);
//void load_buffer(void);

//
// Defines
//
#define RESULTS_BUFFER_SIZE 	 100U
#define EX_ADC_RESOLUTION         16U

#define EPWM_TIMER_TBPRD  		1000U
//#define EPWM_MAX_CMP      		 950U
//#define EPWM_MIN_CMP       		  50U

//#define EPWM_CMP_UP         	   1U
//#define EPWM_CMP_DOWN        	   0U

#define EPWM_CYCLES				 833U
#define ADC_SAMPLE_CYCLES		  50U

//
// Globals
//

typedef struct
{
    volatile struct EPWM_REGS *EPwmRegHandle;
    Uint16 EPwm_CMPA_Direction;
    Uint16 EPwm_CMPB_Direction;
    Uint16 EPwmTimerIntCount;
    Uint16 EPwmMaxCMPA;
    Uint16 EPwmMinCMPA;
    Uint16 EPwmMaxCMPB;
    Uint16 EPwmMinCMPB;
}EPWM_INFO;

EPWM_INFO epwm_info;

Uint16 AdcaResults[RESULTS_BUFFER_SIZE];	//Capacitor Voltage Readings
Uint16 AdcbResults[RESULTS_BUFFER_SIZE];	//Load-Side Current Readings
Uint16 resultsIndex;
volatile Uint16 bufferFull;

//PWM Signals
float theta, modIndex;
Uint16 EPWM_cycle_count;	//Used to index through sine table
Uint16 ADC_cycle_count;		//Used to count number of PWM cycles to set sampling frequency

//void update_compare(EPWM_INFO *epwm_info);

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
    //InitGpio(); // Skipped for this example

    //
    // enable PWM1 and PWM2
    //
    CpuSysRegs.PCLKCR2.bit.EPWM1=1;
    CpuSysRegs.PCLKCR2.bit.EPWM2=1;

    //
    // For this case just init GPIO pins for ePWM1 and ePWM2
    // These functions are in the F2837xD_EPwm.c file
    //
    InitEPwm1Gpio();
    InitEPwm2Gpio();

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

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;

    InitEPwm();

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;

//
// Configure the ADC and power it up
//
    ConfigureADC();

//
// Configure the ePWM
//
    //ConfigureEPWM();

//
// Setup the ADC for ePWM triggered conversions on channels 0 and 2
//
    SetupADC(0, 2);

//
// Enable global Interrupts and higher priority real-time debug events:
//
    IER |= (M_INT1 & M_INT3); //Enable group 1 and 3 interrupts

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
    PieCtrlRegs.PIEIER3.bit.INTx1 = 1;
    //PieCtrlRegs.PIEIER3.bit.INTx2 = 1;
    //PieCtrlRegs.PIEIER3.bit.INTx3 = 1;

//
// 	Allow EPWM TBCTRs to count
//
    /*EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;*/

    EINT;  // Enable Global interrupt INTM
    ERTM;  // Enable Global realtime interrupt DBGM


    //Global definitions
    theta = Sin_tab[0];
    modIndex = 1;

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
// SetupADCEpwm - Setup ADC EPWM acquisition window
//
void SetupADC(Uint16 channelA, Uint16 channelB)
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
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0; //end of SOC0 will set INT1 flag
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;   //enable INT1 flag
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; //make sure INT1 flag is cleared

    //Channel B - Load-Side Current
    AdcbRegs.ADCSOC0CTL.bit.CHSEL = channelB;
    AdcbRegs.ADCSOC0CTL.bit.ACQPS = acqps; //sample window is 100 SYSCLK cycles
	//AdcbRegs.ADCINTSEL1N2.bit.INT1SEL = 0; //end of SOC0 will set INT1 flag
	//AdcbRegs.ADCINTSEL1N2.bit.INT1E = 1;   //enable INT1 flag
	//AdcbRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; //make sure INT1 flag is cleared
    EDIS;
}

__interrupt void epwm1_isr(void)
{
    //
    // Update the CMPA and CMPB values using SPWM modulation
    //
	//update_compare(&epwm_info);



    if(ADC_cycle_count < ADC_SAMPLE_CYCLES)
    	++ADC_cycle_count;
    else
    {
    	ADC_cycle_count = 0;
    	AdcaRegs.ADCSOCFRC1.bit.SOC0 = 1;		//On certain number of PWM Cycles,
    	AdcbRegs.ADCSOCFRC1.bit.SOC0 = 1;		//trigger SOC via software trigger
    }

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
__interrupt void adca1_isr(void)
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
    EPwm1Regs.CMPA.bit.CMPA = 500;//EPWM_MIN_CMP;    // Set compare A value
    EPwm1Regs.CMPB.bit.CMPB = 0;    // Set Compare B value

    EPwm2Regs.CMPA.bit.CMPA = 500;    // Set compare A value
    EPwm2Regs.CMPB.bit.CMPB = 0;    // Set Compare B value

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
    EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;		// Clear PWM1A on event A, up count
    EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;          //Set PWM1A on event A, down count
    EPwm1Regs.AQCTLB.bit.CAU = AQ_CLEAR;
    EPwm1Regs.AQCTLB.bit.CAD = AQ_SET;

    EPwm2Regs.AQCTLA.bit.CAU = AQ_SET;		//Set PWM1A on event A, up count
    EPwm2Regs.AQCTLA.bit.CAD = AQ_CLEAR;	//Clear PWM1A on event A, down count
    EPwm2Regs.AQCTLB.bit.CAU = AQ_SET;
    EPwm2Regs.AQCTLB.bit.CAD = AQ_CLEAR;

    EPwm1Regs.AQCTLA.bit.CAU = AQ_SET;
    EPwm1Regs.AQCTLA.bit.CAD = AQ_CLEAR;
    EPwm1Regs.AQCTLB.bit.CAU = AQ_SET;
    EPwm1Regs.AQCTLB.bit.CAD = AQ_CLEAR;

    EPwm2Regs.AQCTLA.bit.CAU = AQ_CLEAR;
    EPwm2Regs.AQCTLA.bit.CAD = AQ_SET;
    EPwm2Regs.AQCTLB.bit.CAU = AQ_CLEAR;
    EPwm2Regs.AQCTLB.bit.CAD = AQ_SET;

    //
    // Interrupt where we will change the Compare Values
    //
    EPwm1Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;     // Select INT on Zero event
    EPwm1Regs.ETSEL.bit.INTEN = 1;                // Enable INT
    EPwm1Regs.ETPS.bit.INTPRD = ET_1ST;           // Generate INT on 1st event

    epwm_info.EPwm_CMPA_Direction = EPWM_CMP_UP;   // Start by increasing CMPA
	epwm_info.EPwm_CMPB_Direction = EPWM_CMP_DOWN; // & decreasing CMPB
	epwm_info.EPwmTimerIntCount = 0;               // Zero the interrupt counter
	epwm_info.EPwmRegHandle = &EPwm1Regs;          // Set the pointer to the
                                                        // ePWM module
	epwm_info.EPwmMaxCMPA = EPWM_MAX_CMP;        // Setup min/max CMPA/CMPB
                                                        // values
	epwm_info.EPwmMinCMPA = EPWM_MIN_CMP;
	epwm_info.EPwmMaxCMPB = EPWM_MAX_CMP;
	epwm_info.EPwmMinCMPB = EPWM_MIN_CMP;
}

void InitSpi(void)
{
   //
   // Initialize SPI FIFO registers
   //
   SpiaRegs.SPICCR.bit.SPISWRESET = 0; // Reset SPI
   SpiaRegs.SPICCR.all = 0x001F;       //16-bit character, Loopback mode
   SpiaRegs.SPICTL.all = 0x0017;       //Interrupt enabled, Master/Slave XMIT
                                       //enabled
   SpiaRegs.SPISTS.all = 0x0000;
   SpiaRegs.SPIBRR.bit.SPI_BIT_RATE = 0x0063;   // Baud rate
   SpiaRegs.SPIFFTX.all = 0xC022;               // Enable FIFO's, set TX FIFO
                                                // level to 4
   SpiaRegs.SPIFFRX.all = 0x0022;               // Set RX FIFO level to 4
   SpiaRegs.SPIFFCT.all = 0x00;
   SpiaRegs.SPIPRI.all = 0x0000;
   SpiaRegs.SPICCR.bit.SPISWRESET = 1;          // Enable SPI
   SpiaRegs.SPIFFTX.bit.TXFIFO = 1;
   SpiaRegs.SPIFFRX.bit.RXFIFORESET = 1;

   //
   // A DMA transfer will be triggered here!
   //

   //
   // Load the SPI FIFO Tx Buffer
   //
   //load_buffer();

   //
   // Disable the clock to prevent continuous transfer / DMA triggers
   // Note this method of disabling the clock should not be used if
   // actual data is being transmitted
   //
   //EALLOW;
   //CpuSysRegs.PCLKCR8.bit.SPI_A = 0;
   //EDIS;
}

//
// End of file
//

/*void update_compare(EPWM_INFO *epwm_info)
{
	/*
    //
    // Every 10'th interrupt, change the CMPA/CMPB values
    //
    if(epwm_info->EPwmTimerIntCount == 10)
    {
        epwm_info->EPwmTimerIntCount = 0;
        /*delTheta = ((0xFFFF)*40*3.1415927*frequency*EPWM_TIMER_TBPRD*(0.00000001));
        theta = theta + delTheta;
        reference = (int16)modIndex * cos((float)theta/65535);
        compareValue = reference + EPWM_TIMER_TBPRD/2;
        if(compareValue < EPWM_MIN_CMP)
        	epwm_info->EPwmRegHandle->CMPA.bit.CMPA = EPWM_MIN_CMP;
        else if(compareValue > EPWM_MAX_CMP)
        	epwm_info->EPwmRegHandle->CMPA.bit.CMPA = EPWM_MAX_CMP;
        else
        	epwm_info->EPwmRegHandle->CMPA.bit.CMPA = compareValue;
    }
    else
    {
        epwm_info->EPwmTimerIntCount++;
    }

    return;
}*/
