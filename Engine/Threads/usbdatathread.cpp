//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.3.2
//
//  Copyright (c) 2020-2024 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include "usbdatathread.h"

#include <fmt/core.h>

#include <QElapsedTimer>
#include <cstdint>
#include <iostream>

#include "rhxglobals.h"



USBDataThread::USBDataThread(
    AbstractRHXController *controller_, DataStreamFifo *usbFifo_, QObject *parent
)
    : QThread(parent),
      errorChecking(controller_->acquisitionMode() != PlaybackMode),
      controller(controller_),
      usbFifo(usbFifo_),
      keepGoing(false),
      running(false),
      stopThread(false),
      numUsbBlocksToRead(1),
      usbBufferIndex(0)
{
    bufferSize =
        (BufferSizeInBlocks + 1) * BytesPerWord *
        RHXDataBlock::dataBlockSizeInWords(controller->getType(), controller->maxNumDataStreams());
    memoryNeededGB = sizeof(uint8_t) * bufferSize / (1024.0 * 1024.0 * 1024.0);
    cout << "USBDataThread: Allocating " << bufferSize / 1.0e6 << " MBytes for USB buffer." << '\n';
    usbBuffer = nullptr;

    memoryAllocated = true;
    try {
        usbBuffer = new uint8_t[bufferSize];
    } catch (std::bad_alloc &) {
        memoryAllocated = false;
        cerr << "Error: USBDataThread constructor could not allocate " << memoryNeededGB
             << " GB of memory." << '\n';
    }

    // cout << "Ideal thread count: " << QThread::idealThreadCount() << EndOfLine;
}

USBDataThread::~USBDataThread() { delete[] usbBuffer; }

inline std::size_t get_xdaq_frame_size(ControllerType type, int streams)
{
    std::size_t ws = 0;
    if (type == ControllerRecordUSB3)
        ws = 4 + 2 + (streams * 35) + ((streams + 2) % 4) + 8 + 2 + 2;
    else if (type == ControllerStimRecord)
        ws = 4 + 2 + streams * (2 * 20 + 4) + 2 + 8 + 8 + 2 + 2;
    return ws * 2;
}

void USBDataThread::run()
{
    emit hardwareFifoReport(0.0);
    while (!stopThread) {
        QElapsedTimer fifoReportTimer;
        // QElapsedTimer workTimer, loopTimer, reportTimer;
        if (keepGoing) {
            emit hardwareFifoReport(0.0);
            running = true;
            int numBytesRead = 0;
            int bytesInBuffer = 0;
            ControllerType type = controller->getType();
            int numBytesPerDataFrame =
                BytesPerWord *
                RHXDataBlock::dataBlockSizeInWords(type, controller->getNumEnabledDataStreams()) /
                RHXDataBlock::samplesPerDataBlock(type);
            const auto xdaq_frame_size =
                get_xdaq_frame_size(type, controller->getNumEnabledDataStreams());
            int ledArray[8] = {1, 0, 0, 0, 0, 0, 0, 0};
            int ledIndex = 0;
            if (type == ControllerRecordUSB2) {
                // Turn LEDs on to indicate that data acquisition is running.
                controller->setLedDisplay(ledArray);
            }

            controller->setStimCmdMode(true);
            controller->setContinuousRunMode(true);
            controller->run();
            fifoReportTimer.start();
            // loopTimer.start();
            // workTimer.start();
            // reportTimer.start();
            // double usbDataPeriodNsec = 1.0e9 * ((double) numUsbBlocksToRead) *
            //                            ((double) RHXDataBlock::samplesPerDataBlock(type)) /
            //                            controller->getSampleRate();
            const auto streams = controller->getNumEnabledDataStreams();

            auto newStream =
                controller->start_read_stream(0xa0, [&](auto&& event) {
                if(!std::holds_alternative<xdaq::DataStream::Events::OwnedData>(event)) return;
                auto&& data = std::get<xdaq::DataStream::Events::OwnedData>(event);
                auto&& rawData = data.buffer;
                auto&& length = data.length;
                    std::copy(rawData.get(), rawData.get() + length, usbBuffer + usbBufferIndex);
                    bytesInBuffer = usbBufferIndex + length;

                    if (!errorChecking) {
                        // If not checking for USB data glitches, just write all the data to
                        // the FIFO buffer.
                        // TODO: read 32-channel digital IO
                        if (!usbFifo->writeToBuffer(
                                &usbBuffer[usbBufferIndex], (length + usbBufferIndex) / BytesPerWord
                            )) {
                            cerr << "USBDataThread: USB FIFO overrun (1)." << '\n';
                        }
                        usbBufferIndex = 0;
                    } else {
                        usbBufferIndex = 0;
                        // Otherwise, check each USB data block for the correct header bytes
                        // before writing.
                        while (usbBufferIndex <=
                               bytesInBuffer - xdaq_frame_size - USBHeaderSizeInBytes) {
                            if (RHXDataBlock::checkUsbHeader(usbBuffer, usbBufferIndex, type) &&
                                RHXDataBlock::checkUsbHeader(
                                    usbBuffer, usbBufferIndex + xdaq_frame_size, type
                                )) {
                                const auto frame = usbBuffer + usbBufferIndex;
                                if (type == ControllerRecordUSB3) {
                                    const auto dio_off = xdaq_frame_size - 8;
                                    const auto io_off = dio_off - 16;
                                    const auto pad_off = io_off - ((streams + 2) % 4) * 2;
                                    if (!usbFifo->writeToBuffer(
                                            frame + 0,
                                            (pad_off - 0 + (streams % 4) * 2) / BytesPerWord
                                        )) {
                                        cerr << "USBDataThread: USB FIFO overrun (2)." << '\n';
                                    }
                                    frame[dio_off + 2] = frame[dio_off + 4];
                                    frame[dio_off + 3] = frame[dio_off + 5];
                                    if (!usbFifo->writeToBuffer(
                                            frame + io_off, (16 + 4) / BytesPerWord
                                        )) {
                                        cerr << "USBDataThread: USB FIFO overrun (2)." << '\n';
                                    }
                                } else if (type == ControllerStimRecord) {
                                    const auto dio_off = xdaq_frame_size - 8;
                                    const auto io_off = dio_off - 16 - 16;
                                    const auto pad_off = io_off - 4;
                                    // write magic ~ amplifiers to buffer
                                    if (!usbFifo->writeToBuffer(
                                            frame + 0, (pad_off - 0) / BytesPerWord
                                        )) {
                                        cerr << "USBDataThread: USB FIFO overrun (2)." << '\n';
                                    }
                                    // move 32 DIO as 16 DIO
                                    frame[dio_off + 2] = frame[dio_off + 4];
                                    frame[dio_off + 3] = frame[dio_off + 5];
                                    // skip 4 bytes padding, write AIO / 16 DIO to buffer
                                    if (!usbFifo->writeToBuffer(
                                            frame + io_off, (16 + 16 + 4) / BytesPerWord
                                        )) {
                                        cerr << "USBDataThread: USB FIFO overrun (2)." << '\n';
                                    }
                                }
                                usbBufferIndex += xdaq_frame_size;
                            } else {
                                // If headers are not found, advance word by word until we
                                // find them
                                usbBufferIndex += 2;
                            }
                        }
                        // If any data remains in usbBuffer, shift it to the front.
                        if (usbBufferIndex > 0) {
                            int j = 0;
                            for (int i = usbBufferIndex; i < bytesInBuffer; ++i) {
                                usbBuffer[j++] = usbBuffer[i];
                            }
                            usbBufferIndex = j;
                        } else {
                            // If usbBufferIndex == 0, we didn't have enough data to work
                            // with; append more.
                            usbBufferIndex = bytesInBuffer;
                        }
                        if (usbBufferIndex + numBytesRead >= bufferSize) {
                            cerr << "USBDataThread: USB buffer overrun (3)." << '\n';
                        }
                    }

                    bool hasBeenUpdated = false;
                    unsigned int wordsInFifo = controller->getLastNumWordsInFifo(hasBeenUpdated);
                    if (hasBeenUpdated || (fifoReportTimer.nsecsElapsed() > qint64(50e6))) {
                        double fifoPercentageFull = 100.0 * wordsInFifo / FIFOCapacityInWords;
                        emit hardwareFifoReport(fifoPercentageFull);
                        fifoReportTimer.restart();
                        // cout << "Opal Kelly FIFO is " << (int) fifoPercentageFull << "%
                        // full." << EndOfLine;
                    }

                    if (type == ControllerRecordUSB2) {
                        // Advance LED display
                        ledArray[ledIndex] = 0;
                        ledIndex++;
                        if (ledIndex == 8) ledIndex = 0;
                        ledArray[ledIndex] = 1;
                        controller->setLedDisplay(ledArray);
                    }

                    // double workTime = (double) workTimer.nsecsElapsed();
                    // double loopTime = (double) loopTimer.nsecsElapsed();
                    // workTimer.restart();
                    // loopTimer.restart();
                    // if (reportTimer.elapsed() >= 2000) {
                    //     double cpuUsage = 100.0 * workTime / loopTime;
                    //     cout << "UsbDataThread CPU usage: " << (int) cpuUsage << "%" <<
                    //     std::endl; double relativeSpeed = 100.0 * workTime /
                    //     usbDataPeriodNsec; cout << "UsbDataThread speed relative to USB
                    //     data rate: " << (int) relativeSpeed << "%" << std::endl;
                    //     reportTimer.restart();
                    // }
                });

            while (keepGoing && !stopThread) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            if (!newStream) {
                cerr << "Failed to start stream..." << endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            controller->setContinuousRunMode(false);
            controller->setStimCmdMode(false);
            controller->setMaxTimeStep(0);
            controller->flush();  // Flush USB FIFO on Opal Kelly board.
            usbBufferIndex = 0;

            if (type == ControllerRecordUSB2) {
                // Turn off LEDs.
                for (int i = 0; i < 8; ++i) ledArray[i] = 0;
                controller->setLedDisplay(ledArray);
            }

            running = false;
        } else {
            usleep(100);
        }
    }
}

void USBDataThread::startRunning() { keepGoing = true; }

void USBDataThread::stopRunning() { keepGoing = false; }

void USBDataThread::close()
{
    keepGoing = false;
    stopThread = true;
}

bool USBDataThread::isActive() const { return running; }

void USBDataThread::setNumUsbBlocksToRead(int numUsbBlocksToRead_)
{
    if (numUsbBlocksToRead_ > BufferSizeInBlocks) {
        cerr << "USBDataThread::setNumUsbBlocksToRead: Buffer is too small to read "
             << numUsbBlocksToRead_ << " blocks.  Increase BUFFER_SIZE_IN_BLOCKS." << '\n';
    }
    numUsbBlocksToRead = numUsbBlocksToRead_;
}

void USBDataThread::setErrorCheckingEnabled(bool enabled) { errorChecking = enabled; }
