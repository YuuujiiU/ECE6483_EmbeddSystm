# 6483-Just-Breathe

## Members

- Yujie Yu, Net ID yy3913
- Weijie Li, Net ID wl2369
- Tong Wu, Net ID tw2593
- Junda Ai, Net ID ja4426

## Setup

This project uses the library [mbed-goncaloc/ADXL_I2C](https://os.mbed.com/users/goncaloc/code/ADXL_I2C/), to install it as dependency, run the following command:

```bash
pio pkg install
```

Upon successful installation of the dependency, you might get the following error:

> identifier "wait_ms" is undefined

Quote from [arm mbed API reference about wait](https://os.mbed.com/docs/mbed-os/v6.15/apis/wait.html):

> The function wait is deprecated in favor of explicit sleep functions. To sleep, replace wait with ThisThread::sleep_for (C++) or thread_sleep_for (C). To wait (without sleeping), call wait_us. wait_us is safe to call from ISR context.

Change the 3 affected functions in `ADXL_I2C.cpp` to the following per reference above:

```cpp
int ADXL345::oneByteRead(int address) {

    char rx[1];
    rx[0] = address;
    i2c_ -> write((ADXL345_I2C_ADDRESS << 1) & 0xFE, rx, 1);
    ThisThread::sleep_for(1ms); // change
    i2c_ -> read((ADXL345_I2C_ADDRESS << 1) | 0x01, rx, 1);
    ThisThread::sleep_for(1ms) // change
    return rx[0];
}

void ADXL345::oneByteWrite(int address, char data) {

    char tx[2];
    tx[0] = address;
    tx[1] = data;
    i2c_ -> write((ADXL345_I2C_ADDRESS << 1) & 0xFE, tx, 2);
    ThisThread::sleep_for(1ms) // change

}

void ADXL345::multiByteRead(int address, char* data, int bytes) {

    char cmd[6];
    cmd[0] = address;
    i2c_ -> write((ADXL345_I2C_ADDRESS << 1) & 0xFE, cmd, 1);
    ThisThread::sleep_for(1ms) // change
    i2c_ -> read((ADXL345_I2C_ADDRESS << 1) | 0x01, data, bytes );
    ThisThread::sleep_for(1ms) // change

}
```
