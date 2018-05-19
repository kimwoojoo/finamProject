# finalProject

Youtube link : https://youtu.be/r938XKQ_ZP8  
Monitor temperature and lightness on every 1ms

Use an analogous data for a ligthness sensor. 

Build a server in cloud and install MySQL and Apache into it. 

(you can use any sort of a cloud service or use your desktop as a server)

Send sensor data from Raspberry Pi to the server every 10s. 

Turn on FAN when the temperature goes beyond 20 degrees (C)  for 5 second. 

Turn on LED when the lightness goes below ____ and turn off it otherwise. 

Each functionality should be performed by an independant thread (use multi-threaded programming, 4 threads) 

Use a signal mechanism when threads need to communicate each other. 

Collect data for more than 1 minute. 

Capture the screen showing data statistics on the web server. 

(This screen shot should be included in a git repository and a final report). 


pi : gcc -o SFREAL FinalSmartFarm.c -lwiringPi -lpthread -lmysqlclient 
pi : ./SFCREAL

![1](https://user-images.githubusercontent.com/30142775/40271870-d6a6479e-5bde-11e8-90d6-d60260ac2cd0.png)

pi : 127.0.0.1/WoojooDB.php

![2](https://user-images.githubusercontent.com/30142775/40271872-dad3bafe-5bde-11e8-8e62-92037f4ebfe3.png)
