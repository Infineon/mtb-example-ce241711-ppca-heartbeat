/***************************************************************************//**
* \file main.c
* \version 1.0
*
* \brief
* Demonstrates  PPCA core test
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


/*******************************************************************************
* Macros
*******************************************************************************/
#define TIMER_MAX_VALUE 0xFFFFFFFF

/* These are the addresses that the other application running on main core should be using to print. */
#define PPCA_M1_VAR_ADDRESS 0x20040400

/*******************************************************************************
* Global Variables
*******************************************************************************/

volatile uint32_t current_time = 0;
volatile bool heartbeat_flag = true;
volatile bool ignore_first = true;
volatile bool toggle_pin = true;

uint32_t *var = (uint32_t *)PPCA_M1_VAR_ADDRESS;

/* Scheduler timer interrupt config*/
cy_stc_sysint_t scheduler_intr_config =
{
    .intrSrc      = SCHEDULER_TIMER_IRQ,
    .intrPriority = 1U
};

/* Scheduler timer interrupt config*/
cy_stc_sysint_t heartbeat_intr_config =
{
    .intrSrc      = HEARTBEAT_TIMER_IRQ,
    .intrPriority = 1U
};

/* Button interrupt configure */
const cy_stc_sysint_t BTN1_IN_COMBINER_Interrupt_Config =
{
    .intrSrc = EPU_BLK_IRQ_EPU_0, //running on PPCA core value is 5
    .intrPriority = 1U
};

// Define the task structure
typedef struct {
    void (*task_func)(void);
    uint32_t period;
    uint32_t deadline;
    uint8_t priority;
} task_t;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void scheduler_timer_init(void);
void BTN1_IN_COMBINER_Interrupt_Handler(void);
void heartbeat_timer_init(void);
void heartbeat_intr_handler(void);
void scheduler_intr_handler(void);
void scheduler(void);
void print_task(void);
void heartbeat_task(void);

/* Define the task list */
task_t task_list[] = {
    {print_task, 1000000, 1000000, 1}, // print task every 1s
    {heartbeat_task, 500000, 500000, 2} // led task every 500ms
};

/*******************************************************************************
* Function Definitions
*******************************************************************************/

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
    /* EPU configuration */
    Cy_PPCA_EPU_EnableExclusiveAccess(EPU_BLK_HW, true);
    Cy_PPCA_EPU_Enable(EPU_BLK_HW);

    Cy_PPCA_EPU_PU_T1_Configure(HEARTBEAT_PU_T1_HW, HEARTBEAT_PU_T1_INDEX, &HEARTBEAT_PU_T1_put1_config);
    Cy_PPCA_EPU_PU_T1_Enable(HEARTBEAT_PU_T1_HW, HEARTBEAT_PU_T1_INDEX, HEARTBEAT_PU_T1_ENABLE_MODE);
    Cy_PPCA_EPU_Combo_Configure(HEARTBEAT_COMBINER_HW, HEARTBEAT_COMBINER_INDEX, &HEARTBEAT_COMBINER_combo_config);

    /* Button interrupt trigger configure */
    Cy_PPCA_EPU_PU_T2_Configure(BTN1_PU_T2_HW, BTN1_PU_T2_INDEX, &BTN1_PU_T2_put2_config);
    Cy_PPCA_EPU_PU_T2_Enable(BTN1_PU_T2_HW, BTN1_PU_T2_INDEX, BTN1_PU_T2_ENABLE_MODE);
    Cy_PPCA_EPU_Combo_Configure(BTN1_IN_COMBINER_HW, BTN1_IN_COMBINER_INDEX, &BTN1_IN_COMBINER_combo_config);

    /* Configure Processing unit type2 (PUT_2) to generate the constant output */
    Cy_PPCA_EPU_PU_T2_Configure(PUT_IO_HW, PUT_IO_INDEX, &PUT_IO_put2_config);
    Cy_PPCA_EPU_PU_T2_Enable(PUT_IO_HW, PUT_IO_INDEX, PUT_IO_ENABLE_MODE);
    /* Configure Combiner to connect the PUT_2 output to external GPIO (P4.1 LED2) */
    Cy_PPCA_EPU_Combo_Configure(COMBINER_IO_HW, COMBINER_IO_INDEX, &COMBINER_IO_combo_config);

    NVIC_ClearPendingIRQ((IRQn_Type)BTN1_IN_COMBINER_Interrupt_Config.intrSrc);
    Cy_SysInt_Init(&BTN1_IN_COMBINER_Interrupt_Config, &BTN1_IN_COMBINER_Interrupt_Handler);
    NVIC_EnableIRQ((IRQn_Type) BTN1_IN_COMBINER_Interrupt_Config.intrSrc);

    Cy_PPCA_EPU_InterruptSourceSelect(EPU_BLK_EPU_IRQ0_HW, false, epuIrqSrc0);
    Cy_PPCA_EPU_SetInterruptMask(EPU_BLK_EPU_IRQ0_HW);

    /* Enable interrupts */
    __enable_irq();

    scheduler_timer_init();
    heartbeat_timer_init();

    for(;;)
    {
        scheduler();
    }
}

/*******************************************************************************
 * Function Name: scheduler_timer_init
 ********************************************************************************
 * Summary:
 * This function creates and configures a Timer object. The timer ticks
 * continuously and produces a periodic interrupt on every terminal count
 * event. The period is defined by the 'period' and 'compare_value' of the
 * timer configuration structure 'SCHEDULER_TIMER_config'. Without any changes,
 * this application is designed to produce an interrupt every 1 micro second.
 *
 * Parameters:
 *  none
 *
 *******************************************************************************/

void scheduler_timer_init(void)
{
    /*TCPWM Counter Mode initial*/
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(SCHEDULER_TIMER_HW, SCHEDULER_TIMER_NUM, &SCHEDULER_TIMER_config))
    {
         CY_ASSERT(0);
    }

    /* Enable the initialized counter */
    Cy_TCPWM_Counter_Enable(SCHEDULER_TIMER_HW, SCHEDULER_TIMER_NUM);

    /* Configure GPIO interrupt */
    Cy_TCPWM_SetInterruptMask(SCHEDULER_TIMER_HW, SCHEDULER_TIMER_NUM, CY_GPIO_INTR_EN_MASK);

    NVIC_ClearPendingIRQ((IRQn_Type)scheduler_intr_config.intrSrc);

    Cy_SysInt_Init(&scheduler_intr_config, scheduler_intr_handler);

    NVIC_EnableIRQ((IRQn_Type)scheduler_intr_config.intrSrc);

    /* Start the counter */
    Cy_TCPWM_TriggerStart_Single(SCHEDULER_TIMER_HW, SCHEDULER_TIMER_NUM);
}

/*******************************************************************************
 * Function Name: heartbeat_timer_init
 ********************************************************************************
 * Summary:
 * This function creates and configures a Timer object. The timer ticks
 * in one shot mode and produces a periodic interrupt on every terminal count
 * event. The period is defined by the 'period' and 'compare_value' of the
 * timer configuration structure 'HEARTBEAT_TIMER_config'. Without any changes,
 * this application is designed to produce an interrupt every 2 seconds.
 *
 * Parameters:
 *  none
 *
 *******************************************************************************/
void heartbeat_timer_init(void)
{
    /*TCPWM Counter Mode initial*/
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(HEARTBEAT_TIMER_HW, HEARTBEAT_TIMER_NUM, &HEARTBEAT_TIMER_config))
    {
         CY_ASSERT(0);
    }

    /* Enable the initialized counter */
    Cy_TCPWM_Counter_Enable(HEARTBEAT_TIMER_HW, HEARTBEAT_TIMER_NUM);

    /* Configure GPIO interrupt */
    Cy_TCPWM_SetInterruptMask(HEARTBEAT_TIMER_HW, HEARTBEAT_TIMER_NUM, CY_GPIO_INTR_EN_MASK);

    NVIC_ClearPendingIRQ((IRQn_Type)heartbeat_intr_config.intrSrc);

    Cy_SysInt_Init(&heartbeat_intr_config, heartbeat_intr_handler);

    NVIC_EnableIRQ((IRQn_Type)heartbeat_intr_config.intrSrc);

    /* Start the counter */
    Cy_TCPWM_TriggerStart_Single(HEARTBEAT_TIMER_HW, HEARTBEAT_TIMER_NUM);
}

/*******************************************************************************
* Function Name: heartbeat_intr_handler - Heartbeat
********************************************************************************
* Summary:
* This function is the TCPWM interrupt handler . This ISR
* will kill the scheduler timer
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/

/* Timer terminal count interrupt service routine */
void heartbeat_intr_handler(void)
{
    /* Clear the interrupt */
    Cy_TCPWM_ClearInterrupt(HEARTBEAT_TIMER_HW, HEARTBEAT_TIMER_NUM, CY_TCPWM_INT_ON_TC);

    /* Inform Main core through shared memory to bring system to normal state */
    *var = 0XAA;
}

/*******************************************************************************
* Function Name: scheduler_intr_handler - Scheduler
********************************************************************************
* Summary:
* This function is the TCPWM interrupt handler . This ISR
* increment the current time variable
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/

/* Timer terminal count interrupt service routine */
void scheduler_intr_handler(void)
{
    /* Clear the interrupt */
    Cy_TCPWM_ClearInterrupt(SCHEDULER_TIMER_HW, SCHEDULER_TIMER_NUM, CY_TCPWM_INT_ON_TC);

    /* Set the interrupt flag and process it from the main while(1) loop */
    current_time = ((current_time+1) % TIMER_MAX_VALUE);
}

/* Define the scheduler function */
void scheduler(void)
{
    /* Iterate through the task list */
    for (int i = 0; i < sizeof(task_list) / sizeof(task_list[0]); i++)
    {
        /* Check if the task is due */
        if (current_time >= task_list[i].deadline)
        {
            /* Execute the task */
            task_list[i].task_func();

            /* Update the task's deadline */
            task_list[i].deadline = ((task_list[i].deadline + task_list[i].period) % TIMER_MAX_VALUE);
        }
    }
}

/*******************************************************************************
* Function Name: print_task - Scheduler
********************************************************************************
* Summary:
* This function is used to print the core alive message
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/

void print_task(void)
{
    /* Inform Main core through shared memory to Print message using SCB2 DEBUG_UART */
    *var = 0X1;

    /* Inform Main core through shared memory to bring system to normal state */

}

/*******************************************************************************
* Function Name: heartbeat_task - Scheduler
********************************************************************************
* Summary:
* This function is used to reload the heartbeat timer and blink LED
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void heartbeat_task(void)
{
    toggle_pin = !toggle_pin;

    /* Change the GPIO state using the PUT_2 constant_in source */
    Cy_PPCA_EPU_PU_T2_Generate_SW_Event(PUT_IO_HW, PUT_IO_INDEX, toggle_pin);

    if(heartbeat_flag)
    {
        /* Relaod the Heartbeat Timer*/
        Cy_TCPWM_TriggerReloadOrIndex_Single(HEARTBEAT_TIMER_HW, HEARTBEAT_TIMER_NUM);
    }
}

/*******************************************************************************
* Function Name: BTN1_IN_COMBINER_Interrupt_Handler
********************************************************************************
* Summary:
* This function is the Button handler . This ISR
* set flag to stop Heartbeat timer.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/

/* Timer terminal count interrupt service routine */
void BTN1_IN_COMBINER_Interrupt_Handler(void)
{
    Cy_PPCA_EPU_ClearInterrupt(EPU_BLK_EPU_IRQ0_HW);  //Using 0th EPU IRQ

    if(!ignore_first)
    {
        /* set flag to stop Heartbeat timer */
        heartbeat_flag = false;
    }

    ignore_first = false;
}
