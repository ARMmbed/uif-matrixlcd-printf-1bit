/* mbed
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed-drivers/mbed.h"

#include "uif-matrixlcd/MatrixLCDImplementation.h"

/// Private Constants
enum PrivateConstants{
    LCD_Stride_Bytes = 18,  // bytes
    LCD_Width_Bits   = 128, // bits (px) wide
    LCD_Width_Bytes  = 16,  // bytes wide
    LCD_Height_Rows  = 128, // rows (px) tall
    LCD_End_Padding  = 2    // bytes to make sure last line is written
};

/// Private Data
static __attribute__((aligned (4))) uint16_t LCD_Buffer_1[LCD_Stride_Bytes * LCD_Height_Rows / 2 + LCD_End_Padding] = {0};
static __attribute__((aligned (4))) uint16_t LCD_Buffer_2[LCD_Stride_Bytes * LCD_Height_Rows / 2 + LCD_End_Padding] = {0};

static uint16_t* LCD_Front_Buffer = LCD_Buffer_1; // front buffer is displayed
static uint16_t* LCD_Back_Buffer  = LCD_Buffer_2; // back buffer is drawn into

/// Private Function Declarations
static void initControlSignals(uint16_t* buf);
static uint8_t* startOfPixels(uint16_t* buf);

/// Public API Implementation
MatrixLCDImplementation::MatrixLCDImplementation()
    :   MatrixLCDBase(),
        onTransferBegin(),
        onTransferDone(),
        frameIndex(0)
{
    // write control signals to frame buffer
    initControlSignals(LCD_Front_Buffer);
    initControlSignals(LCD_Back_Buffer);

    frameBuffer[0] = SharedPointer<FrameBuffer>(new FrameBuffer1Bit(startOfPixels(LCD_Front_Buffer),
                                                                    LCD_Width_Bits,
                                                                    LCD_Height_Rows,
                                                                    LCD_Stride_Bytes));

    frameBuffer[1] = SharedPointer<FrameBuffer>(new FrameBuffer1Bit(startOfPixels(LCD_Back_Buffer),
                                                                    LCD_Width_Bits,
                                                                    LCD_Height_Rows,
                                                                    LCD_Stride_Bytes));
}

// pointer to the first pixels
// pointer to the first pixels
SharedPointer<FrameBuffer> MatrixLCDImplementation::getFrameBuffer()
{
    frameIndex = frameIndex ^ 0x01;

    return frameBuffer[frameIndex];
}

// send the front buffer to the screen
void MatrixLCDImplementation::sendFrameBuffer(SharedPointer<FrameBuffer>& buffer, FunctionPointer _onTransferBegin, FunctionPointer _onTransferDone)
{
    // store callbacks
    onTransferBegin = _onTransferBegin;
    onTransferDone = _onTransferDone;

    // select buffer
    uint16_t* address = NULL;

    if (buffer.get() == frameBuffer[0].get())
    {
        address = LCD_Front_Buffer;
    }
    else
    {
        address = LCD_Back_Buffer;
    }

    // call transfer function
    FunctionPointer1<void, uint16_t*> fp(this, &MatrixLCDImplementation::transfer);
    minar::Scheduler::postCallback(fp.bind(address))
        .tolerance(minar::milliseconds(0));
}

void MatrixLCDImplementation::transfer(uint16_t* address)
{
    // call external callback signaling transfer has begun
    if (onTransferBegin)
    {
        onTransferBegin.call();
    }

    // clear screen and move cursor to (0,0)
    printf("%c[2J%c[;H", 27, 27);

    uint8_t* buffer = (uint8_t*) address;

    for (size_t height = 0; height < LCD_Height_Rows; height++)
    {
        for (size_t width = 2; width < LCD_Width_Bytes + 2; width++)
        {
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (buffer[height * LCD_Stride_Bytes + width] & (1 << bit))
                {
                    printf("<>");
                }
                else
                {
                    printf("  ");
                }
            }
        }

        printf("\r\n");
    }

    FunctionPointer0<void> fp(this, &MatrixLCDImplementation::transferDone);
    minar::Scheduler::postCallback(fp);
}

void MatrixLCDImplementation::transferDone()
{
    // schedule original callback function to be called
    minar::Scheduler::postCallback(onTransferDone)
        .tolerance(minar::milliseconds(0));
}


/// Private Function Definitions
static void initControlSignals(uint16_t* buf)
{
    uint8_t* bufb = (uint8_t*)buf;

    for(int i=0; i<LCD_Height_Rows; i++)
    {
        // use the "update" command to start every line – if this isn't the
        // first line sent within the transfer it will be ignored anyway
        bufb[0] = 0x01;
        // 1-indexed address of line
        bufb[1] = i+1;

        for(int j = 2; j < LCD_Stride_Bytes; j++)
            bufb[j] = 0xa5;

        bufb += LCD_Stride_Bytes;
    }
}

static uint8_t* startOfPixels(uint16_t* buf)
{
    return ((uint8_t*) buf) + 2;
}
