/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define GPIO_HFXT_PORT                                                     GPIOA
#define GPIO_HFXIN_PIN                                             DL_GPIO_PIN_5
#define GPIO_HFXIN_IOMUX                                         (IOMUX_PINCM10)
#define GPIO_HFXOUT_PIN                                            DL_GPIO_PIN_6
#define GPIO_HFXOUT_IOMUX                                        (IOMUX_PINCM11)
#define CPUCLK_FREQ                                                     80000000
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  2000
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);


/* Defines for PWMA */
#define PWMA_INST                                                          TIMG7
#define PWMA_INST_IRQHandler                                    TIMG7_IRQHandler
#define PWMA_INST_INT_IRQN                                      (TIMG7_INT_IRQn)
#define PWMA_INST_CLK_FREQ                                              40000000
/* GPIO defines for channel 0 */
#define GPIO_PWMA_C0_PORT                                                  GPIOA
#define GPIO_PWMA_C0_PIN                                          DL_GPIO_PIN_28
#define GPIO_PWMA_C0_IOMUX                                        (IOMUX_PINCM3)
#define GPIO_PWMA_C0_IOMUX_FUNC                       IOMUX_PINCM3_PF_TIMG7_CCP0
#define GPIO_PWMA_C0_IDX                                     DL_TIMER_CC_0_INDEX

/* Defines for PWMB */
#define PWMB_INST                                                          TIMA0
#define PWMB_INST_IRQHandler                                    TIMA0_IRQHandler
#define PWMB_INST_INT_IRQN                                      (TIMA0_INT_IRQn)
#define PWMB_INST_CLK_FREQ                                              40000000
/* GPIO defines for channel 1 */
#define GPIO_PWMB_C1_PORT                                                  GPIOB
#define GPIO_PWMB_C1_PIN                                          DL_GPIO_PIN_20
#define GPIO_PWMB_C1_IOMUX                                       (IOMUX_PINCM48)
#define GPIO_PWMB_C1_IOMUX_FUNC                      IOMUX_PINCM48_PF_TIMA0_CCP1
#define GPIO_PWMB_C1_IDX                                     DL_TIMER_CC_1_INDEX



/* Defines for Motor_PID */
#define Motor_PID_INST                                                   (TIMA1)
#define Motor_PID_INST_IRQHandler                               TIMA1_IRQHandler
#define Motor_PID_INST_INT_IRQN                                 (TIMA1_INT_IRQn)
#define Motor_PID_INST_LOAD_VALUE                                        (7999U)




/* Defines for OLED */
#define OLED_INST                                                           I2C1
#define OLED_INST_IRQHandler                                     I2C1_IRQHandler
#define OLED_INST_INT_IRQN                                         I2C1_INT_IRQn
#define OLED_BUS_SPEED_HZ                                                 100000
#define GPIO_OLED_SDA_PORT                                                 GPIOB
#define GPIO_OLED_SDA_PIN                                          DL_GPIO_PIN_3
#define GPIO_OLED_IOMUX_SDA                                      (IOMUX_PINCM16)
#define GPIO_OLED_IOMUX_SDA_FUNC                       IOMUX_PINCM16_PF_I2C1_SDA
#define GPIO_OLED_SCL_PORT                                                 GPIOB
#define GPIO_OLED_SCL_PIN                                          DL_GPIO_PIN_2
#define GPIO_OLED_IOMUX_SCL                                      (IOMUX_PINCM15)
#define GPIO_OLED_IOMUX_SCL_FUNC                       IOMUX_PINCM15_PF_I2C1_SCL


/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           40000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                        DL_GPIO_PIN_11
#define GPIO_UART_0_TX_PIN                                        DL_GPIO_PIN_10
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM22)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM21)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM21_PF_UART0_TX
#define UART_0_BAUD_RATE                                                 (11520)
#define UART_0_IBRD_40_MHZ_11520_BAUD                                      (217)
#define UART_0_FBRD_40_MHZ_11520_BAUD                                        (1)





/* Port definition for Pin Group LED */
#define LED_PORT                                                         (GPIOB)

/* Defines for LED_TEST: GPIOB.22 with pinCMx 50 on package pin 21 */
#define LED_LED_TEST_PIN                                        (DL_GPIO_PIN_22)
#define LED_LED_TEST_IOMUX                                       (IOMUX_PINCM50)
/* Defines for AIN1: GPIOA.13 with pinCMx 35 on package pin 6 */
#define Motor_l_AIN1_PORT                                                (GPIOA)
#define Motor_l_AIN1_PIN                                        (DL_GPIO_PIN_13)
#define Motor_l_AIN1_IOMUX                                       (IOMUX_PINCM35)
/* Defines for AIN2: GPIOB.26 with pinCMx 57 on package pin 28 */
#define Motor_l_AIN2_PORT                                                (GPIOB)
#define Motor_l_AIN2_PIN                                        (DL_GPIO_PIN_26)
#define Motor_l_AIN2_IOMUX                                       (IOMUX_PINCM57)
/* Defines for E1A: GPIOB.23 with pinCMx 51 on package pin 22 */
#define Motor_l_E1A_PORT                                                 (GPIOB)
// groups represented: ["Motor_2","Motor_l"]
// pins affected: ["E2A","E1A"]
#define GPIO_MULTIPLE_GPIOB_INT_IRQN                            (GPIOB_INT_IRQn)
#define GPIO_MULTIPLE_GPIOB_INT_IIDX            (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define Motor_l_E1A_IIDX                                    (DL_GPIO_IIDX_DIO23)
#define Motor_l_E1A_PIN                                         (DL_GPIO_PIN_23)
#define Motor_l_E1A_IOMUX                                        (IOMUX_PINCM51)
/* Defines for E1B: GPIOB.12 with pinCMx 29 on package pin 64 */
#define Motor_l_E1B_PORT                                                 (GPIOB)
#define Motor_l_E1B_PIN                                         (DL_GPIO_PIN_12)
#define Motor_l_E1B_IOMUX                                        (IOMUX_PINCM29)
/* Port definition for Pin Group Motor_2 */
#define Motor_2_PORT                                                     (GPIOB)

/* Defines for BIN1: GPIOB.9 with pinCMx 26 on package pin 61 */
#define Motor_2_BIN1_PIN                                         (DL_GPIO_PIN_9)
#define Motor_2_BIN1_IOMUX                                       (IOMUX_PINCM26)
/* Defines for BIN2: GPIOB.7 with pinCMx 24 on package pin 59 */
#define Motor_2_BIN2_PIN                                         (DL_GPIO_PIN_7)
#define Motor_2_BIN2_IOMUX                                       (IOMUX_PINCM24)
/* Defines for E2A: GPIOB.4 with pinCMx 17 on package pin 52 */
#define Motor_2_E2A_IIDX                                     (DL_GPIO_IIDX_DIO4)
#define Motor_2_E2A_PIN                                          (DL_GPIO_PIN_4)
#define Motor_2_E2A_IOMUX                                        (IOMUX_PINCM17)
/* Defines for E2B: GPIOB.5 with pinCMx 18 on package pin 53 */
#define Motor_2_E2B_PIN                                          (DL_GPIO_PIN_5)
#define Motor_2_E2B_IOMUX                                        (IOMUX_PINCM18)
/* Defines for X1: GPIOA.27 with pinCMx 60 on package pin 31 */
#define TRACE_X1_PORT                                                    (GPIOA)
#define TRACE_X1_PIN                                            (DL_GPIO_PIN_27)
#define TRACE_X1_IOMUX                                           (IOMUX_PINCM60)
/* Defines for X2: GPIOB.27 with pinCMx 58 on package pin 29 */
#define TRACE_X2_PORT                                                    (GPIOB)
#define TRACE_X2_PIN                                            (DL_GPIO_PIN_27)
#define TRACE_X2_IOMUX                                           (IOMUX_PINCM58)
/* Defines for X3: GPIOA.12 with pinCMx 34 on package pin 5 */
#define TRACE_X3_PORT                                                    (GPIOA)
#define TRACE_X3_PIN                                            (DL_GPIO_PIN_12)
#define TRACE_X3_IOMUX                                           (IOMUX_PINCM34)
/* Defines for X4: GPIOB.8 with pinCMx 25 on package pin 60 */
#define TRACE_X4_PORT                                                    (GPIOB)
#define TRACE_X4_PIN                                             (DL_GPIO_PIN_8)
#define TRACE_X4_IOMUX                                           (IOMUX_PINCM25)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_PWMA_init(void);
void SYSCFG_DL_PWMB_init(void);
void SYSCFG_DL_Motor_PID_init(void);
void SYSCFG_DL_OLED_init(void);
void SYSCFG_DL_UART_0_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
