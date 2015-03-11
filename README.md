# seasound-solo-wdm
The Solo WDM driver

This is the WDM part of the seasound solo driver. Posted here for the people who want to recompile it 
to support 64-bit Windows 7/8. And, I suppose, if anybody wants to enhance the XP driver support
and create a pull request, which I will happily integrate, I guess. Although that seems unlikely.

If you are looking for instructions on how to manipulate the magical registers in a Seasound
audio card while writing your own driver, look in this source. If not, er, what are you
doing here? Go get a beer or something!

ASIO and EASI components are omitted due to copyright restrictions.
Some microsoft WDM boilerplate is included. Note that this may only 
be used in creation of a WDM driver for use with Microsoft. My code
may be used to create as many WDM drivers for Windows XP as you want, 
or in other drivers, or indeed for any other purpose you want,
freely with no restrictions. However, since the Microsoft boilerplate
code, described in its documentation as something intended to be copied
into the project and used thusly, has a specific closed-source license,
the entire project can not be officially, openly BSD licensed. 

I personally recommend the Focusrite Scarlett recording interfaces as an excellent
personal recording interface. Only one headphone jack, but it's pretty and sounds great.
