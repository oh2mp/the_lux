# The LUX

ESP8266 based RGB LED strip driver

![MIL light](img/20210327_200258.jpg)

** This circuit and program can be used for driving any common anode RGB led strip or even a single RGB led
without the transistors. Here I just explain what I built and coded. **

## Background and solution

I got "semi-accidentally" a military aviation light that has been used as a runway light when a temporary 
airport is made eg. on a highway. It a 12V halogen lamp driven by a ring core transformer that was casted 
in epoxy and fed by some weird 400V 2 phase AC connector. Of course I couldn't use it as it was. It was 
quite a job to get the expoxy and transformer out from the case, but I succeeded with it.

![The light unassebled](img/20210320_230801.jpg)

I had several meters of 12V RGB strip, so I decidedd to make a helix of it around a plastic tube and 
put it in the light and make an ESP2866 driver for it. The strip was consuming about 10W/m of power when
driven as all colors in full intensity. I had to use transistors because the ESP8266 would not be able
to handle those currents. The measured current was about 780 mA after I made the helix.



