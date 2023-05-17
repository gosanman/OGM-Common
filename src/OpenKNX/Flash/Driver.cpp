#include "OpenKNX/Common.h"
#include "OpenKNX/Flash/Driver.h"

#ifdef ARDUINO_ARCH_SAMD
extern uint32_t __etext;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
#else
extern uint32_t _EEPROM_start;
extern uint32_t _FS_start;
extern uint32_t _FS_end;
#endif

namespace OpenKNX
{
    namespace Flash
    {
        Driver::Driver(uint32_t offset, uint32_t size, std::string id)
        {
            _id = id;
            _offset = offset;
            _size = size;

#ifdef ARDUINO_ARCH_SAMD
            const uint32_t pageSizes[] = {8, 16, 32, 64, 128, 256, 512, 1024};
            _sectorSize = pageSizes[NVMCTRL->PARAM.bit.PSZ] * 4;
            _endFree = pageSizes[NVMCTRL->PARAM.bit.PSZ] * NVMCTRL->PARAM.bit.NVMP;
            _startFree = (uint32_t)(&__etext + (&__data_end__ - &__data_start__)); // text + data MemoryBlock
#else
            _sectorSize = FLASH_SECTOR_SIZE;
            // Full Size
            // _maxSize = (uint32_t)(&_EEPROM_start) - 0x10000000lu + 4096lu;
            // Size up to EEPROM
            // _maxSize = (uint32_t)(&_EEPROM_start) - 0x10000000lu;
            // Size up to FS (if FS 0 it = _EEPROM_start)
            _endFree = (uint32_t)(&_FS_start) - 0x10000000lu;
#endif

            validateParameters();
        }

        std::string Driver::logPrefix()
        {
            return openknx.logger.logPrefix("FlashDriver", _id);
        }

        void Driver::printBaseInfo()
        {
            logInfoP("initalize %i bytes at 0x%X", _size, _offset);
            logIndentUp();
            logDebugP("sectorSize: %i", _sectorSize);
            logDebugP("startFree: %i", _startFree);
            logDebugP("endFree: %i", _endFree);
            logIndentDown();
        }

        void Driver::validateParameters()
        {
            if (_size % _sectorSize)
                fatalError(1, "Flash: Size unaligned");
            if (_offset % _sectorSize)
                fatalError(1, "Flash: Offset unaligned");
            if (_size > _endFree)
                fatalError(1, "Flash: End behind free flash");
            if (_offset < _startFree)
            {
                logInfoP("%i < %i", _offset, _startFree);
                fatalError(1, "Flash: Offset start before free flash begin");
            }
        }

        uint8_t *Driver::flashAddress()
        {
#ifdef ARDUINO_ARCH_SAMD
            return (uint8_t *)_offset;
#else
            return (uint8_t *)XIP_BASE + _offset;
#endif
        }

        uint32_t Driver::sectorSize()
        {
            return _sectorSize;
        }

        uint32_t Driver::startFree()
        {
            return _startFree;
        }

        uint32_t Driver::endFree()
        {
            return _endFree;
        }

        uint32_t Driver::size()
        {
            return _size;
        }

        uint32_t Driver::startOffset()
        {
            return _offset;
        }

        uint16_t Driver::sectorOfRelativeAddress(uint32_t relativeAddress)
        {
            return relativeAddress / _sectorSize;
        }

        bool Driver::needEraseSector(uint16_t sector)
        {
            for (size_t i = 0; i < _sectorSize; i++)
                if ((flashAddress() + sector * _sectorSize)[i] != 0xFF)
                    return true;

            return false;
        }

        bool Driver::needWriteSector()
        {
            return memcmp(_buffer, flashAddress() + _bufferSector * _sectorSize, _sectorSize);
        }

        bool Driver::needEraseForBuffer()
        {
            uint8_t flashByte;
            uint8_t bufferByte;
            for (size_t i = 0; i < _sectorSize; i++)
            {
                flashByte = (flashAddress() + _bufferSector * _sectorSize)[i];
                bufferByte = _buffer[i];

                if (bufferByte != flashByte && (bufferByte & ~flashByte))
                    return true;
            }

            return false;
        }

        void Driver::loadSector(uint16_t sector, bool force /* = false */)
        {
            // skip - already loaded and not force
            if (!force && _buffer != nullptr && sector == _bufferSector)
                return;

            // load specific sector
            logTraceP("load buffer for sector %i", sector);
            logIndentUp();

            // an other sector is loaded - commit before load
            if (_buffer != nullptr && sector != _bufferSector)
                commit();

            // initalize buffer for first time
            if (_buffer == nullptr)
                _buffer = new uint8_t[_sectorSize];

            _bufferSector = sector;
            memcpy(_buffer, flashAddress() + _bufferSector * _sectorSize, _sectorSize);
            logIndentDown();
        }

        void Driver::commit()
        {
            // no sector loaded
            if (_buffer == nullptr)
                return;

            logTraceP("commit");
            logIndentUp();
            writeSector();
            logIndentDown();
        }

        uint32_t Driver::write(uint32_t relativeAddress, uint8_t value, uint32_t size /* = 1 */)
        {
            if (size <= 0)
                return relativeAddress;

            uint16_t sector = sectorOfRelativeAddress(relativeAddress);

            // load buffer if needed
            loadSector(sector);

            // position in loaded buffer
            uint16_t bufferPosition = relativeAddress % _sectorSize;

            // determine available large within the sector
            uint16_t writeMaxSize = _sectorSize - bufferPosition;

            // determine size outside the sector
            uint16_t overheadSize = (writeMaxSize < size) ? size - writeMaxSize : 0;

            // determine how much must be stored within the sector.
            uint16_t writeSize = (writeMaxSize < size) ? writeMaxSize : size;

            // write date to current buffer
            memset(_buffer + bufferPosition, value, writeSize);

            // write overhead in next sector
            if (overheadSize > 0)
                return write(relativeAddress + writeSize, value, overheadSize);

            return relativeAddress + size;
        }

        uint32_t Driver::write(uint32_t relativeAddress, uint8_t *buffer, uint32_t size /* = 1 */)
        {
            if (size <= 0)
                return relativeAddress;

            uint16_t sector = sectorOfRelativeAddress(relativeAddress);

            // load buffer if needed
            loadSector(sector);

            // position in loaded buffer
            uint16_t bufferPosition = relativeAddress % _sectorSize;

            // determine available large within the sector
            uint16_t writeMaxSize = _sectorSize - bufferPosition;

            // determine size outside the sector
            uint16_t overheadSize = (writeMaxSize < size) ? size - writeMaxSize : 0;

            // determine how much must be stored within the sector.
            uint16_t writeSize = (writeMaxSize < size) ? writeMaxSize : size;

            // write date to current buffer
            memcpy(_buffer + bufferPosition, buffer, writeSize);

            // write overhead in next sector
            if (overheadSize > 0)
                return write(relativeAddress + writeSize, buffer + writeSize, overheadSize);

            return relativeAddress + size;
        }

        uint32_t Driver::writeByte(uint32_t relativeAddress, uint8_t value)
        {
            return write(relativeAddress, (uint8_t *)&value, 1);
        }

        uint32_t Driver::writeWord(uint32_t relativeAddress, uint16_t value)
        {
            return write(relativeAddress, (uint8_t *)&value, 2);
        }

        uint32_t Driver::writeInt(uint32_t relativeAddress, uint32_t value)
        {
            return write(relativeAddress, (uint8_t *)&value, 4);
        }

        uint32_t Driver::read(uint32_t relativeAddress, uint8_t *output, uint32_t size)
        {
            memcpy(output, flashAddress() + relativeAddress, size);
            return relativeAddress + 1;
        }

        uint8_t Driver::readByte(uint32_t relativeAddress)
        {
            uint8_t buffer = 0;
            read(relativeAddress, &buffer, 1);
            return buffer;
        }

        uint16_t Driver::readWord(uint32_t relativeAddress)
        {
            uint16_t buffer = 0;
            read(relativeAddress, (uint8_t *)&buffer, 2);
            return buffer;
        }

        uint32_t Driver::readInt(uint32_t relativeAddress)
        {
            uint32_t buffer = 0;
            read(relativeAddress, (uint8_t *)&buffer, 4);
            return buffer;
        }

        void Driver::eraseSector(uint16_t sector)
        {
            if (!needEraseSector(sector))
            {
                logTraceP("skip erase sector, because already erased");
                return;
            }

            logTraceP("erase sector %i", sector);

#ifdef ARDUINO_ARCH_SAMD
            NVMCTRL->ADDR.reg = ((uint32_t)_offset + (sector * _sectorSize)) / 2;
            NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
            while (!NVMCTRL->INTFLAG.bit.READY)
            {
            }
#else
            noInterrupts();
            rp2040.idleOtherCore();
            flash_range_erase(_offset + (sector * _sectorSize), _sectorSize);
            rp2040.resumeOtherCore();
            interrupts();
#endif
        }

        void Driver::writeSector()
        {
            if (!needWriteSector())
            {
                logTraceP("skip write sector, because no changes");
                return;
            }

            if (needEraseForBuffer())
            {
                eraseSector(_bufferSector);
            }

            logTraceP("write sector %i", _bufferSector);
            // logHexTraceP(_buffer, _sectorSize);

#ifdef ARDUINO_ARCH_SAMD
            // logHexTraceP(_buffer, _sectorSize);
            volatile uint32_t *src_addr = (volatile uint32_t *)_buffer;
            volatile uint32_t *dst_addr = (volatile uint32_t *)(flash() + (_bufferSector * _sectorSize));

            // Disable automatic page write
            NVMCTRL->CTRLB.bit.MANW = 1;

            uint16_t size = _sectorSize / 4;

            // Do writes in pages
            while (size)
            {
                // Execute "PBC" Page Buffer Clear
                NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
                while (NVMCTRL->INTFLAG.bit.READY == 0)
                {
                }

                // Fill page buffer
                for (uint16_t i = 0; i < (_sectorSize / 16) && size; i++)
                {
                    *dst_addr = *src_addr;
                    src_addr++;
                    dst_addr++;
                    size--;
                }

                // Execute "WP" Write Page
                NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WP;
                while (NVMCTRL->INTFLAG.bit.READY == 0)
                {
                }
            }
#else
            noInterrupts();
            rp2040.idleOtherCore();

            // write smaller FLASH_PAGE_SIZE to reduze write time
            uint32_t currentPosition = 0;
            uint32_t currentSize = 0;
            while (currentPosition < _sectorSize)
            {
                while (memcmp(_buffer + currentPosition + currentSize, flashAddress() + (_bufferSector * _sectorSize) + currentPosition + currentSize, FLASH_PAGE_SIZE))
                {
                    currentSize += FLASH_PAGE_SIZE;

                    // last
                    if (currentPosition + currentSize == _sectorSize)
                        break;
                }

                // Changes Found
                if (currentSize > 0)
                    flash_range_program(_offset + (_bufferSector * _sectorSize) + currentPosition, _buffer + currentPosition, currentSize);

                currentPosition += currentSize + FLASH_PAGE_SIZE;
                currentSize = 0;
            }
            rp2040.resumeOtherCore();
            interrupts();
#endif
        }
    } // namespace Flash
} // namespace OpenKNX