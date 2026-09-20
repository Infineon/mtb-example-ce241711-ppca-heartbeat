/***************************************************************************//**
* \file main.c
* \version 1.0
*
* \brief
* Demonstrates  PPCA Heartbeat
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
*******************************************************************************/

/* These are the addresses where the core0 and core1 images are located. */
#define CORE0_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca0_nvm_C_S_START    //  0x12030000
#define CORE1_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca1_nvm_C_S_START    //  0x12038000
#define PPCA0_IMAGE_SIZE       CYMEM_CM33_0_S_ppca0_code_SIZE
#define PPCA1_IMAGE_SIZE       CYMEM_CM33_0_S_ppca1_code_SIZE

/* These are the addresses that the other application running on PPCA cores should be using to update. */
#define PPCA_M1_VAR_ADDRESS 0x53050400

/*******************************************************************************
* Global Variables
*******************************************************************************/

/* Debug UART variables */
static cy_stc_scb_uart_context_t    DEBUG_UART_context; /* DEBUG_UART context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj; /* Debug DEBUG_UART HAL object */

const cy_stc_ppca_cnfg_ppcaout_input_selector_t ppca_input_btn1 = {
    .inputSelSrc = PPCAIN_SEL_SRC_0,
    .disSynchronizerStage = true,
};

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This code example demonstrates the use of PPCA TCPWM as heartbeat timer to 
* monitor status PPCA core is active and use of PPCA timer for scheduling events
* in each CPU subsystem. Heartbeat timer is configured as single shot mode and 
* scheduler has to reload the timer before expiry otherwise scheduler timer 
* will be killed and send signal to main core to take appropriate action.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);
    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL DEBUG_UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config,
                                &DEBUG_UART_context, NULL);

    /* HAL DEBUG_UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize redirecting of low level IO */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* retarget IO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: PPCA heartbeat\r\n");
    printf("************************************************************\r\n\n");

    /* enable interrupts */
    __enable_irq();

    /* Programming PPCA core*/
    Cy_System_Init_CPU0((void*)CORE0_IMAGE_ADDRESS, PPCA0_IMAGE_SIZE);
    Cy_System_Init_CPU1((void*)CORE1_IMAGE_ADDRESS, PPCA1_IMAGE_SIZE);

    printf("PPCA CM33 cores Boot called\n");

    uint32_t *ppca_core0_var = (uint32_t *)PPCA_M1_VAR_ADDRESS;
    *ppca_core0_var = 0;

    /* Enable PPCA block */
    Cy_PPCA_CNFG_Init(CNFG_TCPWM_OUT_HW, &CNFG_TCPWM_OUT_config);
    Cy_PPCA_Enable(CNFG_TCPWM_OUT_HW);
    Cy_PPCA_CNFG_PPCA_Output_Selector(CNFG_TCPWM_OUTCNFG_HW, &CNFG_TCPWM_OUT_ppcaOutConfig);
    Cy_PPCA_CNFG_PPCA_Input_Selector(CNFG_TCPWM_OUTCNFG_HW, &ppca_input_btn1, 0);

    for (;;)
    {

         if(*ppca_core0_var == 0xAA)
         {
            printf("\r\nHeartbeat: PPCA Core0 is not Alive");
         }

         if(*ppca_core0_var == 0x1)
         {
            printf("\r\nHeartBeat: PPCA CORE0 is Alive!");
            *ppca_core0_var = 0x0;
         }

         Cy_SysLib_Delay(1000);
    }
}
