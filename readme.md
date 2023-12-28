Digital Level Example Using Active Objects and the Super Simple Tasker
===============

I wrote this example toy application for the STM32F407G-Disc1 development board. The main goal was to learn about the following items from Miro Sameks excellent [Modern Embedded Systems Programming](https://www.youtube.com/watch?v=hnj-7XwTYRI) course. 

The intention was to put into practice the following items from the course. 
1. [Active objects](https://www.youtube.com/watch?v=l69ghMpsp6w).
2. [The Super Simple Tasker Kernel](https://www.youtube.com/watch?v=PTcauYl994A). 
3. [State machines](https://www.youtube.com/watch?v=EBSxZKjgBXI) (although not the [Optimal](https://www.youtube.com/watch?v=FCymm6PBtOs) implementation described in the course).
4. [Object orientated design patterns](https://www.youtube.com/watch?v=dSLodtKuung) in C. 

The project has the following main modules. All using SST active object tasks (non blocking and run to completion).
1. spi_manager: manages a single spi peripheral (as a master). This will allows multiple threads to share the same spi peripheral and multiple slaves (the chip select pin is handled in the manager) concurently without the need for inter-communication or mutual exclusion mechanisms. Each thread requests access to the spi mananger via rxtx (for now) request events pushed into the spi_manager threads message queue. The driver is written in a object orientated way to allow multiple manager instances to be created to handle more than one peripheral on the microcontroller.
2. LIS3DSH: Communicates with the mems accelerometer on the DISCO1 board. Runs the steps to initialise the board configuration, verify it has been written and then commences the polling of the accelerometer data. Communication is made via non blocking requests to the spi_manager.
3. Blink: Contains a periodic task which runs at 50ms, takes the accelerometer data and illuminates the four LEDs on the DISCO1 board depending on the orientation of the board. 
4. BSP: The board support package configures each of the tasks and links them to their associated interrupt service routines. It also provides initialisation functions for the hardware (some derived from cubeMX) and interface functions to the LEDs.

The repo contains an [stm32cubeide](https://www.st.com/en/development-tools/stm32cubeide.html) project to allow you to build and debug quickly.

## SPI_manager States
The spi manager is implemented as a simple state machine. See the diagram below. Currently, because the message queue in the SST kernel is a queue of pointers to queue items, the txrx jobs in the managers message queue either have to be immutable or left alone and kept valid by the requestor until a txrxComplete or timeout response from the manager has been posted back to the requesting task.
If the manager receives a txrx (I haven't implemented Tx only yet) request signal event during the middle of an SPI transaction the manager populates an internal buffer of requested jobs which it empties when the SPI peripheral becomes available again. 

![alt text](https://github.com/AngryActiveObject/DigitalLevel_SuperSimpleTasker/blob/main/Docs/SPI_Manager.png "SPI_Manager")

## LIS3DSH States
This module communicates via the SPI_manager with the LISD3H mems accelerometer on the board. The initialising states walk through a process of configuration and verification (re attempting if needed) followed by a polling mode driven by an SST_TimerEvt kernal object. 

![alt text](https://github.com/AngryActiveObject/DigitalLevel_SuperSimpleTasker/blob/main/Docs/LIS3DSH_Handler.png "LIS3DSH_Handler.png")

TODO: setup the interrupt mode of operation whereby the MEMs chip triggers a hardware interrupt in the STM32 to report it has fresh acceleration data ready.
## BSP Task configuration 
In the BSP.c package each task object is configured, constructed using the modules constructor method and linked to an ISR as below.

```c

    /*****************************Blinky Task Config************************/

    #define BLINKY_IRQn (79u) /* interrupt line in the NVIQ for unused interrupt on STM32F407*/
    #define BLINKY_IRQHANDLER0 UNUSED_IRQHandler0 /*UNUSED interrupt handler is used for blinky thread*/
    #define BLINKY_TASK_PRIORITY ((SST_TaskPrio)1u) // /*opposite to NVIC increasing numbers have increasing priority*/

    static BlinkyTask_T BlinkyInstance;
    static SST_Task *const AO_Blink = &(BlinkyInstance.super); /*Scheduler task pointer*/

    #define BLINKY_MSG_QUEUELEN (10u)
    static SST_Evt const *blinkyMsgQueue[BLINKY_MSG_QUEUELEN]; /*initialised in the SST_Task_Start function*/

    void BLINKY_IRQHANDLER0(void) {
        SST_Task_activate(AO_Blink); /*trigger the task on interrupt.*/
    }

    void BSP_init_blinky_task(void) {
        Blinky_ctor(&BlinkyInstance);

        SST_Task_setIRQ(AO_Blink, BLINKY_IRQn);

        NVIC_EnableIRQ(BLINKY_IRQn); 

        SST_Task_start(AO_Blink, BLINKY_TASK_PRIORITY, blinkyMsgQueue,
        BLINKY_MSG_QUEUELEN, 0); /*no intial event*/'
    }
```