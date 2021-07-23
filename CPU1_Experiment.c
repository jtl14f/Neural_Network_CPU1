//
// Included Files
//
#include "F28x_Project.h"
//#include <math.h>
//#include "SineTable.h"

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

/*typedef struct
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

EPWM_INFO epwm_info;*/

Uint16 AdcaResults[RESULTS_BUFFER_SIZE];	//Capacitor Voltage Readings
Uint16 AdcbResults[RESULTS_BUFFER_SIZE];	//Load-Side Current Readings
Uint16 resultsIndex;
volatile Uint16 bufferFull;

//PWM Signals
float sin_theta, modIndex;
Uint16 EPWM_cycle_count;	//Used to index through sine table
Uint16 reference;
//int16 reference, modIndex;
//Uint16 theta, delTheta, compareValue, frequency;
Uint16 ADC_cycle_count;		//Used to count number of PWM cycles to set sampling frequency
//int16 testint;

const float Sin_tab[EPWM_CYCLES]=
{0.000000,0.007552,0.015103,0.022654,0.030203,0.037751,0.045296,0.052839,0.060378,0.067915,
0.075447,0.082975,0.090499,0.098017,0.105530,0.113036,0.120537,0.128030,0.135516,0.142994,
0.150465,0.157926,0.165379,0.172822,0.180255,0.187678,0.195090,0.202491,0.209881,0.217259,
0.224624,0.231976,0.239316,0.246641,0.253953,0.261250,0.268532,0.275799,0.283050,0.290285,
0.297503,0.304704,0.311888,0.319055,0.326203,0.333332,0.340443,0.347534,0.354605,0.361656,
0.368686,0.375696,0.382683,0.389650,0.396593,0.403515,0.410413,0.417288,0.424139,0.430965,
0.437768,0.444545,0.451297,0.458023,0.464723,0.471397,0.478043,0.484663,0.491255,0.497818,
0.504354,0.510860,0.517338,0.523786,0.530204,0.536592,0.542949,0.549275,0.555570,0.561834,
0.568065,0.574264,0.580430,0.586563,0.592662,0.598728,0.604760,0.610757,0.616719,0.622646,
0.628538,0.634393,0.640213,0.645996,0.651742,0.657451,0.663123,0.668756,0.674352,0.679909,
0.685427,0.690907,0.696347,0.701747,0.707107,0.712427,0.717706,0.722944,0.728141,0.733296,
0.738410,0.743482,0.748511,0.753497,0.758441,0.763341,0.768198,0.773010,0.777779,0.782504,
0.787183,0.791818,0.796408,0.800952,0.805451,0.809904,0.814310,0.818670,0.822984,0.827250,
0.831470,0.835641,0.839766,0.843842,0.847870,0.851850,0.855781,0.859664,0.863497,0.867281,
0.871016,0.874701,0.878336,0.881921,0.885456,0.888940,0.892374,0.895757,0.899088,0.902368,
0.905597,0.908774,0.911900,0.914973,0.917994,0.920963,0.923880,0.926743,0.929554,0.932312,
0.935016,0.937667,0.940265,0.942809,0.945300,0.947736,0.950119,0.952447,0.954721,0.956940,
0.959105,0.961215,0.963271,0.965271,0.967217,0.969107,0.970942,0.972721,0.974446,0.976114,
0.977727,0.979284,0.980785,0.982231,0.983620,0.984953,0.986230,0.987451,0.988615,0.989724,
0.990775,0.991770,0.992709,0.993591,0.994416,0.995185,0.995897,0.996552,0.997150,0.997691,
0.998176,0.998603,0.998974,0.999287,0.999544,0.999743,0.999886,0.999971,1.000000,0.999971,
0.999886,0.999743,0.999544,0.999287,0.998974,0.998603,0.998176,0.997691,0.997150,0.996552,
0.995897,0.995185,0.994416,0.993591,0.992709,0.991770,0.990775,0.989724,0.988615,0.987451,
0.986230,0.984953,0.983620,0.982231,0.980785,0.979284,0.977727,0.976114,0.974446,0.972721,
0.970942,0.969107,0.967217,0.965271,0.963271,0.961215,0.959105,0.956940,0.954721,0.952447,
0.950119,0.947736,0.945300,0.942809,0.940265,0.937667,0.935016,0.932312,0.929554,0.926743,
0.923880,0.920963,0.917994,0.914973,0.911900,0.908774,0.905597,0.902368,0.899088,0.895757,
0.892374,0.888940,0.885456,0.881921,0.878336,0.874701,0.871016,0.867281,0.863497,0.859664,
0.855781,0.851850,0.847870,0.843842,0.839766,0.835641,0.831470,0.827250,0.822984,0.818670,
0.814310,0.809904,0.805451,0.800952,0.796408,0.791818,0.787183,0.782504,0.777779,0.773010,
0.768198,0.763341,0.758441,0.753497,0.748511,0.743482,0.738410,0.733296,0.728141,0.722944,
0.717706,0.712427,0.707107,0.701747,0.696347,0.690907,0.685427,0.679909,0.674352,0.668756,
0.663123,0.657451,0.651742,0.645996,0.640213,0.634393,0.628538,0.622646,0.616719,0.610757,
0.604760,0.598728,0.592662,0.586563,0.580430,0.574264,0.568065,0.561834,0.555570,0.549275,
0.542949,0.536592,0.530204,0.523786,0.517338,0.510860,0.504354,0.497818,0.491255,0.484663,
0.478043,0.471397,0.464723,0.458023,0.451297,0.444545,0.437768,0.430965,0.424139,0.417288,
0.410413,0.403515,0.396593,0.389650,0.382683,0.375696,0.368686,0.361656,0.354605,0.347534,
0.340443,0.333332,0.326203,0.319055,0.311888,0.304704,0.297503,0.290285,0.283050,0.275799,
0.268532,0.261250,0.253953,0.246641,0.239316,0.231976,0.224624,0.217259,0.209881,0.202491,
0.195090,0.187678,0.180255,0.172822,0.165379,0.157926,0.150465,0.142994,0.135516,0.128030,
0.120537,0.113036,0.105530,0.098017,0.090499,0.082975,0.075447,0.067915,0.060378,0.052839,
0.045296,0.037751,0.030203,0.022654,0.015103,0.007552,0.000000,-0.007552,-0.015103,-0.022654,
-0.030203,-0.037751,-0.045296,-0.052839,-0.060378,-0.067915,-0.075447,-0.082975,-0.090499,-0.098017,
-0.105530,-0.113036,-0.120537,-0.128030,-0.135516,-0.142994,-0.150465,-0.157926,-0.165379,-0.172822,
-0.180255,-0.187678,-0.195090,-0.202491,-0.209881,-0.217259,-0.224624,-0.231976,-0.239316,-0.246641,
-0.253953,-0.261250,-0.268532,-0.275799,-0.283050,-0.290285,-0.297503,-0.304704,-0.311888,-0.319055,
-0.326203,-0.333332,-0.340443,-0.347534,-0.354605,-0.361656,-0.368686,-0.375696,-0.382683,-0.389650,
-0.396593,-0.403515,-0.410413,-0.417288,-0.424139,-0.430965,-0.437768,-0.444545,-0.451297,-0.458023,
-0.464723,-0.471397,-0.478043,-0.484663,-0.491255,-0.497818,-0.504354,-0.510860,-0.517338,-0.523786,
-0.530204,-0.536592,-0.542949,-0.549275,-0.555570,-0.561834,-0.568065,-0.574264,-0.580430,-0.586563,
-0.592662,-0.598728,-0.604760,-0.610757,-0.616719,-0.622646,-0.628538,-0.634393,-0.640213,-0.645996,
-0.651742,-0.657451,-0.663123,-0.668756,-0.674352,-0.679909,-0.685427,-0.690907,-0.696347,-0.701747,
-0.707107,-0.712427,-0.717706,-0.722944,-0.728141,-0.733296,-0.738410,-0.743482,-0.748511,-0.753497,
-0.758441,-0.763341,-0.768198,-0.773010,-0.777779,-0.782504,-0.787183,-0.791818,-0.796408,-0.800952,
-0.805451,-0.809904,-0.814310,-0.818670,-0.822984,-0.827250,-0.831470,-0.835641,-0.839766,-0.843842,
-0.847870,-0.851850,-0.855781,-0.859664,-0.863497,-0.867281,-0.871016,-0.874701,-0.878336,-0.881921,
-0.885456,-0.888940,-0.892374,-0.895757,-0.899088,-0.902368,-0.905597,-0.908774,-0.911900,-0.914973,
-0.917994,-0.920963,-0.923880,-0.926743,-0.929554,-0.932312,-0.935016,-0.937667,-0.940265,-0.942809,
-0.945300,-0.947736,-0.950119,-0.952447,-0.954721,-0.956940,-0.959105,-0.961215,-0.963271,-0.965271,
-0.967217,-0.969107,-0.970942,-0.972721,-0.974446,-0.976114,-0.977727,-0.979284,-0.980785,-0.982231,
-0.983620,-0.984953,-0.986230,-0.987451,-0.988615,-0.989724,-0.990775,-0.991770,-0.992709,-0.993591,
-0.994416,-0.995185,-0.995897,-0.996552,-0.997150,-0.997691,-0.998176,-0.998603,-0.998974,-0.999287,
-0.999544,-0.999743,-0.999886,-0.999971,-1.000000,-0.999971,-0.999886,-0.999743,-0.999544,-0.999287,
-0.998974,-0.998603,-0.998176,-0.997691,-0.997150,-0.996552,-0.995897,-0.995185,-0.994416,-0.993591,
-0.992709,-0.991770,-0.990775,-0.989724,-0.988615,-0.987451,-0.986230,-0.984953,-0.983620,-0.982231,
-0.980785,-0.979284,-0.977727,-0.976114,-0.974446,-0.972721,-0.970942,-0.969107,-0.967217,-0.965271,
-0.963271,-0.961215,-0.959105,-0.956940,-0.954721,-0.952447,-0.950119,-0.947736,-0.945300,-0.942809,
-0.940265,-0.937667,-0.935016,-0.932312,-0.929554,-0.926743,-0.923880,-0.920963,-0.917994,-0.914973,
-0.911900,-0.908774,-0.905597,-0.902368,-0.899088,-0.895757,-0.892374,-0.888940,-0.885456,-0.881921,
-0.878336,-0.874701,-0.871016,-0.867281,-0.863497,-0.859664,-0.855781,-0.851850,-0.847870,-0.843842,
-0.839766,-0.835641,-0.831470,-0.827250,-0.822984,-0.818670,-0.814310,-0.809904,-0.805451,-0.800952,
-0.796408,-0.791818,-0.787183,-0.782504,-0.777779,-0.773010,-0.768198,-0.763341,-0.758441,-0.753497,
-0.748511,-0.743482,-0.738410,-0.733296,-0.728141,-0.722944,-0.717706,-0.712427,-0.707107,-0.701747,
-0.696347,-0.690907,-0.685427,-0.679909,-0.674352,-0.668756,-0.663123,-0.657451,-0.651742,-0.645996,
-0.640213,-0.634393,-0.628538,-0.622646,-0.616719,-0.610757,-0.604760,-0.598728,-0.592662,-0.586563,
-0.580430,-0.574264,-0.568065,-0.561834,-0.555570,-0.549275,-0.542949,-0.536592,-0.530204,-0.523786,
-0.517338,-0.510860,-0.504354,-0.497818,-0.491255,-0.484663,-0.478043,-0.471397,-0.464723,-0.458023,
-0.451297,-0.444545,-0.437768,-0.430965,-0.424139,-0.417288,-0.410413,-0.403515,-0.396593,-0.389650,
-0.382683,-0.375696,-0.368686,-0.361656,-0.354605,-0.347534,-0.340443,-0.333332,-0.326203,-0.319055,
-0.311888,-0.304704,-0.297503,-0.290285,-0.283050,-0.275799,-0.268532,-0.261250,-0.253953,-0.246641,
-0.239316,-0.231976,-0.224624,-0.217259,-0.209881,-0.202491,-0.195090,-0.187678,-0.180255,-0.172822,
-0.165379,-0.157926,-0.150465,-0.142994,-0.135516,-0.128030,-0.120537,-0.113036,-0.105530,-0.098017,
-0.090499,-0.082975,-0.075447,-0.067915,-0.060378,-0.052839,-0.045296,-0.037751,-0.030203,-0.022654,
-0.015103,-0.007552,0.000000};


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
    //CpuSysRegs.PCLKCR2.bit.EPWM2=1;

    //
    // For this case just init GPIO pins for ePWM1 and ePWM2
    // These functions are in the F2837xD_EPwm.c file
    //
    //InitEPwm1Gpio();
    //InitEPwm2Gpio();

    EALLOW;

    GpioCtrlRegs.GPEPUD.bit.GPIO145 = 1;    // Disable pull-up on GPIO145 (EPWM1A)
    GpioCtrlRegs.GPEPUD.bit.GPIO146 = 1;    // Disable pull-up on GPIO146 (EPWM1B)

    GpioCtrlRegs.GPEMUX2.bit.GPIO145 = 1;   // Configure GPIO145 as EPWM1A
    GpioCtrlRegs.GPEMUX2.bit.GPIO146 = 1;   // Configure GPIO0146 as EPWM1B

    //GpioCtrlRegs.GPEPUD.bit.GPIO147 = 1;    // Disable pull-up on GPIO147 (EPWM2A)
    //GpioCtrlRegs.GPEPUD.bit.GPIO148 = 1;    // Disable pull-up on GPIO148 (EPWM2B)

    //GpioCtrlRegs.GPEMUX2.bit.GPIO147 = 1;   // Configure GPIO147 as EPWM2A
    //GpioCtrlRegs.GPEMUX2.bit.GPIO148 = 1;   // Configure GPIO148 as EPWM2B

    EDIS;

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

    /*EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;*/

//
// Configure the ADC and power it up
//
    ConfigureADC();

//
// Configure the ePWM
//
    //ConfigureEPWM();

//
// Setup the ADC for ePWM triggered conversions on channels
//
    SetupADC(1, 4);

//
// Enable global Interrupts and higher priority real-time debug events:
//
    IER |= (M_INT1 | M_INT3); //Enable group 1 and 3 interrupts

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

    //Global definitions
    //sin_theta = Sin_tab[0];
    /*frequency = 1;
    theta = 0;
    modIndex = 1;*/
    EPWM_cycle_count = 0;
    ADC_cycle_count = 0;
    modIndex = 1.0;
    //testint = 0;

    EINT;  // Enable Global interrupt INTM
    ERTM;  // Enable Global realtime interrupt DBGM

//
// 	Allow EPWM TBCTRs to count
//
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;

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
        //asm("   ESTOP0");

    } while(1);
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
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);

    AdcbRegs.ADCCTL2.bit.PRESCALE = 6; //set ADCCLK divider to /4
	AdcSetMode(ADC_ADCB, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);

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
	//testint = 1;
    //
    // Update the CMPA and CMPB values using SPWM modulation
    //
	//update_compare(&epwm_info);
	if(EPWM_cycle_count > EPWM_CYCLES)
		EPWM_cycle_count = 0;
	else
		++EPWM_cycle_count;

	sin_theta = Sin_tab[EPWM_cycle_count];
	reference = (Uint16)(((float)EPWM_TIMER_TBPRD * (0.5 + (modIndex * sin_theta / 2.0))));
	EPwm1Regs.CMPA.bit.CMPA = reference;
	//EPwm2Regs.CMPA.bit.CMPA = reference;
	//EPwm1Regs.CMPB.bit.CMPB = reference;
	//EPwm2Regs.CMPA.bit.CMPA = reference;
	/*delTheta = ((0xFFFF)*4*3.1415927*frequency*EPWM_TIMER_TBPRD*(0.00000001));
	theta = theta + delTheta;
	reference = modIndex * sin((float)theta/65535);
	compareValue = reference + EPWM_TIMER_TBPRD/2;
	EPwm1Regs.CMPA.bit.CMPA = compareValue;*/

	/*if(compareValue < EPWM_MIN_CMP)
	        	EPWM_.CMPA.bit.CMPA = EPWM_MIN_CMP;
	        else if(compareValue > EPWM_MAX_CMP)
	        	epwm_info->EPwmRegHandle->CMPA.bit.CMPA = EPWM_MAX_CMP;
	        else
	        	epwm_info->EPwmRegHandle->CMPA.bit.CMPA = compareValue;*/

    if(ADC_cycle_count < ADC_SAMPLE_CYCLES)
    	++ADC_cycle_count;
    else
    {
    	ADC_cycle_count = 0;
    	AdcbRegs.ADCSOCFRC1.bit.SOC0 = 1;		//On certain number of PWM Cycles,
    	AdcaRegs.ADCSOCFRC1.bit.SOC0 = 1;		//trigger SOC via software trigger
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
    AdcbResults[resultsIndex] = AdcbResultRegs.ADCRESULT0;
    AdcaResults[resultsIndex++] = AdcaResultRegs.ADCRESULT0;

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

    //EPwm2Regs.TBPRD = EPWM_TIMER_TBPRD;
    //EPwm2Regs.TBPHS.bit.TBPHS = 0x0000;        // Phase is 0
    //EPwm2Regs.TBCTR = 0x0000;                  // Clear counter

    //
    // Set Compare values
    //
    EPwm1Regs.CMPA.bit.CMPA = 500;  // Set compare A value
    EPwm1Regs.CMPB.bit.CMPB = 0;    // Set Compare B value

    //EPwm2Regs.CMPA.bit.CMPA = 500;    // Set compare A value
    //EPwm2Regs.CMPB.bit.CMPB = 0;    // Set Compare B value

    //
    // Setup counter mode
    //
    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up and down
    EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;        // Disable phase loading
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;       // Clock ratio to SYSCLKOUT
    EPwm1Regs.TBCTL.bit.CLKDIV = TB_DIV1;
    EPwm1Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_IN;

    //EPwm2Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up and down
	//EPwm2Regs.TBCTL.bit.PHSEN = TB_ENABLE;
	//EPwm2Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;       // Clock ratio to SYSCLKOUT
	//EPwm2Regs.TBCTL.bit.CLKDIV = TB_DIV1;
	//EPwm2Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_IN;

    //
    // Setup shadowing
    //
    EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    EPwm1Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO; // Load on Zero
    EPwm1Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    //EPwm2Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    //EPwm2Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    //EPwm2Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO; // Load on Zero
    //EPwm2Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    //
    // Set actions
    //

    EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;
    EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;
    EPwm1Regs.AQCTLB.bit.CAU = AQ_SET;
    EPwm1Regs.AQCTLB.bit.CAD = AQ_CLEAR;

    //EPwm2Regs.AQCTLA.bit.CAU = AQ_SET;		//Set PWM1A on event A, up count
    //EPwm2Regs.AQCTLA.bit.CAD = AQ_CLEAR;	//Clear PWM1A on event A, down count
    //EPwm2Regs.AQCTLB.bit.CAU = AQ_CLEAR;
    //EPwm2Regs.AQCTLB.bit.CAD = AQ_SET;

    /*EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;		// Clear PWM1A on event A, up count
    EPwm1Regs.AQCTLA.bit.CAD = AQ_SET;          //Set PWM1A on event A, down count
    EPwm1Regs.AQCTLB.bit.CAU = AQ_SET;
    EPwm1Regs.AQCTLB.bit.CAD = AQ_CLEAR;

    EPwm2Regs.AQCTLA.bit.CAU = AQ_SET;		//Set PWM1A on event A, up count
    EPwm2Regs.AQCTLA.bit.CAD = AQ_CLEAR;	//Clear PWM1A on event A, down count
    EPwm2Regs.AQCTLB.bit.CAU = AQ_CLEAR;
    EPwm2Regs.AQCTLB.bit.CAD = AQ_SET;*/

    /*EPwm1Regs.AQCTLA.bit.CAU = AQ_SET;
    EPwm1Regs.AQCTLA.bit.CAD = AQ_CLEAR;
    EPwm1Regs.AQCTLB.bit.CAU = AQ_SET;
    EPwm1Regs.AQCTLB.bit.CAD = AQ_CLEAR;

    EPwm2Regs.AQCTLA.bit.CAU = AQ_CLEAR;
    EPwm2Regs.AQCTLA.bit.CAD = AQ_SET;
    EPwm2Regs.AQCTLB.bit.CAU = AQ_CLEAR;
    EPwm2Regs.AQCTLB.bit.CAD = AQ_SET;*/

    //Setup Deadband - 300 ns
    EPwm1Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
    EPwm1Regs.DBCTL.bit.POLSEL = DB_ACTV_HIC;
    EPwm1Regs.DBCTL.bit.IN_MODE = DBA_ALL;
    EPwm1Regs.DBRED.bit.DBRED = 30;				//Corresponds to 300 ns
    EPwm1Regs.DBFED.bit.DBFED = 30;

    //EPwm2Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
    //EPwm2Regs.DBCTL.bit.POLSEL = DB_ACTV_LO;
    //EPwm1Regs.DBCTL.bit.IN_MODE = DBA_ALL;
    //EPwm2Regs.DBRED.bit.DBRED = 30;				//Corresponds to 300 ns

    //
    // Interrupt where we will change the Compare Values
    //
    EPwm1Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;     // Select INT on Zero event
    EPwm1Regs.ETSEL.bit.INTEN = 1;                // Enable INT
    EPwm1Regs.ETPS.bit.INTPRD = ET_1ST;           // Generate INT on 1st event

    /*epwm_info.EPwm_CMPA_Direction = EPWM_CMP_UP;   // Start by increasing CMPA
	epwm_info.EPwm_CMPB_Direction = EPWM_CMP_DOWN; // & decreasing CMPB
	epwm_info.EPwmTimerIntCount = 0;               // Zero the interrupt counter
	epwm_info.EPwmRegHandle = &EPwm1Regs;          // Set the pointer to the
                                                        // ePWM module
	epwm_info.EPwmMaxCMPA = EPWM_MAX_CMP;        // Setup min/max CMPA/CMPB
                                                        // values
	epwm_info.EPwmMinCMPA = EPWM_MIN_CMP;
	epwm_info.EPwmMaxCMPB = EPWM_MAX_CMP;
	epwm_info.EPwmMinCMPB = EPWM_MIN_CMP;*/
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
