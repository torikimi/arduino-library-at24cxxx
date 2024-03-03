
#include "at24cxxx.h"
#include "Arduino.h"

AT24Cxxx::AT24Cxxx(uint8_t address, TwoWire& i2c, int writeDelay, uint16_t size, uint8_t pageSize)  :
  i2cAddress(address), size(size), twoWire(&i2c), writeDelay(writeDelay), pageSize(pageSize) {
}

uint8_t
AT24Cxxx::read( int idx ){
  uint8_t result;
  readBuffer(idx, &result, 1);
  return result;
}

void
AT24Cxxx::write( int idx, uint8_t val){
  uint8_t data = val;
  writeBuffer(idx, &data, 1);
}

void
AT24Cxxx::update( int idx, uint8_t val){
  if (val != read(idx)) {
    write(idx, val);
  }
}

uint16_t
AT24Cxxx::length() {
  return size;
}

 uint8_t 
 AT24Cxxx::getLastError() {
  return lastError;
 }

void
AT24Cxxx::writeAddress(uint16_t address){
  twoWire->beginTransmission(i2cAddress);
  twoWire->write((uint8_t)((address >> 8) & 0xFF));
  twoWire->write((uint8_t)(address & 0xFF));
}

int
AT24Cxxx::rawWriteBuffer(uint16_t address, const uint8_t* data, size_t len){
  lastError = 0;
  writeAddress(address);
  int written = 0;
  for (written = 0; written < len; written++) {
    // Not using twoWire's built in buffer write since it hides errors from write.
    if (twoWire->write(data[written]) != 1) {
      // An error here (not 1) indicates that we have reached the end of the 
      // internal write buffer, so no more data can be written.
      // Stop filling the buffer, write what we got and return the number written
      break;
    }
  }
	lastError = twoWire->endTransmission();
  // The AT24Cxxx chips needs 5-20 ms time after write (tWR Write Cycle Time) 
  // to become available again for new operations. 
  // It is possible to poll the chip to ask if it is ready, but this is hard to
  // do through the TwoWire-API, so instead we just do a hard wait to ensure
  // that the chip is available again before we finish the operation.
  delay(writeDelay); 
  return lastError == 0 ? written : 0;
}

int
AT24Cxxx::writeBuffer(uint16_t address, const uint8_t* data, size_t len){
  const uint8_t* dataToWrite = data;
  size_t lenRemaining = len;
  uint16_t nextAddress = address;
  int totalWritten = 0;
  size_t numberOfWrites = 0;
  do {
    // Since page write to the AT24Cxxx chips only works within the pages
    // we must make sure to split our writes on page borders.
    // Therefore we start by finding out how far it is to the next page
    // border and only write as many bytes in one write operation.
    uint16_t locationOnPage = nextAddress % pageSize;
    size_t maxBytesToWrite = pageSize - locationOnPage;
    size_t bytesToWrite = min(maxBytesToWrite, lenRemaining);
    // Note, due to other internal buffer sizes in the TwoWire libraries
    // we may not be able to write the whole message. Therefore we keep track
    // on how many bytes were actually written and use that number when
    // calculating what to write next
    size_t written = rawWriteBuffer(nextAddress, dataToWrite, bytesToWrite);
    if (getLastError() != 0) {
      // If we got a hard error from the TwoWire bus, there is no point to continue
      break;
    }
    totalWritten += written;
    lenRemaining -= written;
    dataToWrite += written;
    nextAddress += written;
  } while ((lenRemaining > 0) && (++numberOfWrites < len));
  return totalWritten;
}

int
AT24Cxxx::readBuffer(uint16_t address, uint8_t* data, uint8_t len){
  lastError = 0;
  uint8_t* dataPointer = data;
  uint8_t lenRemaining = len;
  uint16_t nextAddress = address;
  int totalread = 0;
  uint8_t numberOfReads = 0;
  do {
    // Since underlying layers will limit how many bytes we can actually read
    // in one go, we will try to read as many as possible, but see from the
    // result how many were actually read, and make multiple reads until
    // we have all data.
    writeAddress(nextAddress);
  	lastError = twoWire->endTransmission();
    if (lastError != 0) {
      // If we got a hard error from the TwoWire bus, there is no point to continue
      break;
    }
  	uint8_t readBytes = twoWire->requestFrom(i2cAddress, lenRemaining);
  	int byteNumber;
  	for(byteNumber = 0; (byteNumber < readBytes) && twoWire->available(); byteNumber++){
  		dataPointer[byteNumber] = twoWire->read();
  	}
    totalread += byteNumber;
    lenRemaining -= byteNumber;
    dataPointer += byteNumber;
    nextAddress += byteNumber;
  } while ((lenRemaining > 0) && (++numberOfReads < len));
  return totalread;
}
