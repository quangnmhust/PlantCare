# SHT25 Library for Arduino by [**Uravu Labs**](http://www.uravulabs.com/index)


#### About SHT25
- Fully calibrated with 1.8%RH accuracy
- Digital output, I2C interface
- Supply voltage 1.8 to 3.6 V
- Operating range: 0 to 100 %RH, -40 to 125°C
- Low power consumption
- Excellent long term stability


# Content
 Latest Release [![GitHubrelease](https://img.shields.io/badge/release-v0.1-blue.svg)](https://github.com/UravuLabs/SHT25/releases/latest/)
- Installation
  - [Arduino IDE](#arduino-ide)
    - [Using Git repository archive](#using-git-repository-archive)
  - [PlatformIO IDE](#platformio-ide)
    - [Using Git version](#using-git-version)
    - [Using PlatformIO library manager](#using-platformio-library-manager)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License and credits](#license-and-credits)

# Arduino IDE
#### Using Git repository archive
- Goto repository [SHT25](https://github.com/UravuLabs/SHT25)
- Click on **Clone or Download** link and select **Download ZIP**
- Open Arduino IDE and select **Sketch** ->**Include Library** ->**Add .ZIP library**
- Restart Arduino IDE and Select **File** -> **Examples** -> **SHT25** for demo examples

# PlatformIO IDE
PlatformIO is an open source ecosystem for IoT development with cross platform build system, library manager and full support for Arduino development. It works on the popular host OS: macOS, Windows, Linux 32/64, Linux ARM (like Raspberry Pi, BeagleBone, CubieBoard).

- [What is PlatformIO?](https://docs.platformio.org/en/latest/what-is-platformio.html#)
- [PlatformIO IDE](https://platformio.org/platformio-ide)
- [PlatformIO Core](http://docs.platformio.org/en/latest/core.html?utm_source=github&utm_medium=sht25) (command line tool)
- [Advanced usage](http://docs.platformio.org/en/latest/platforms/espressif8266.html?utm_source=github&utm_medium=sht25) - custom settings, Over-the-Air (OTA), staging version
- [Integration with Cloud and Standalone IDEs](http://docs.platformio.org/en/latest/ide.html?utm_source=github&utm_medium=sht25) - Cloud9, Codeanywhere, Eclipse Che (Codenvy), Atom, CLion, Eclipse, Emacs, NetBeans, Qt Creator, Sublime Text, VIM, Visual Studio, and VSCode

#### Using Git version
- Open **Atom Editor** -> **PlatformIO Home** -> **Libraries** -> **Registry** -> **Install**
- Past link [***https://github.com/UravuLabs/SHT25.git***]()
- Restart Atom Editor and library is ready import in your project

#### Using PlatformIO library manager
-  Open **Atom Editor** -> **PlatformIO Home** -> **Libraries** -> **Registry**
-  Search for SHT25 and click on Install button
-  Restart Atom Editor and library is ready import in your project

# Documentation
- ``` begin(void) ```

  *Class member function which soft reset sensor.*
- ``` char setResCombination(byte combination) ```

  *Class member function argument must be the combination number given in the table below. It set the resolution combination as well as the delay related to measurement*

|Combi. No.| Bits  | RH Res  |Temp Res   |
|---------|-------|---------|-----------|
|    1    |  00   |   12    |    14     |
|    2    |  01   |   8     |    12     |
|    3    |  10   |   10    |    13     |
|    4    |  11   |   11    |    11     |

- ``` char enableHeater(void)/ char disableHeater(void) ```

  *Class member functions enable and desable onchip heater*
- ``` float getTemperature(void) ```

  *Class member function return temperature value in float*
- ``` float getHumidity(void) ```

  *Class member function return humidity value in float*
- ``` char isEOB(void) ```

  *Class member function return End of Battery status. If Battery voltage is below 2.25V it will return 1*

# Contributing
- If minor bug fixes of code and documentation, please go ahead and submit a pull request.
- Check out the list of issues which are easy to fix.
- Larger changes should generally be discussed by opening an issue first.

# License and Credits

SHT25 library is developed and maintained by the [**Uravu Labs**](http://www.uravulabs.com/index) and licensed under GNU General Public License v2.0.

Arduino IDE is developed and maintained by the Arduino team and it is licensed under GPL.

PlatformIO atom IDE is licensed under Apache 2.0.
