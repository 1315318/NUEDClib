#include "key.h"
extern int status;
uint32_t counter_B = 0;
uint32_t counter_A = 0;


void GPIOB_IRQHandler(void)
{
    switch (DL_GPIO_getPendingInterrupt(GPIOB)) // 注意：GPIOB 必须是宏定义好的端口指针
    {
        case Motor_2_E2A_IIDX:
            counter_B++;
            break;
        case Motor_l_E1A_IIDX:
            counter_A++;
            break;
        default:
            break; // 建议加上 default
    }
}

    

// uint8_t get_key_state(uint32_t key)
// {
//     uint8_t state = DL_GPIO_readPins(KEY_PORT, key);
        // if((high bits & key)! == 0)
        // return 1;
        // else
        // ruturn 0
//     return state;
// }
// void GROUP_IRQHandler()
// {
//     switch(DL_GPIO_getPendingInterrupt(KEY_PORT))
//     {
//         case KEY_KEYX_IIDX:
//             status =  (status + 1) % 3;
//             break;
//         case KEY_KEYX_IID
//              status =  (status + 3 - 1) % 3;
//             break;
//         default:
//             break;
//     }
// }