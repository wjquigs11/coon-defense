# Raccoon Defense System (RDS)

![evil coon](data/coon.jpeg)

I don't like raccoons. They hunt my chickens (sometimes successfully). They tear up the tiny patch of grass in my back yard. They are a nuisance. I don't like them.

I bought a motion sensor sprinkler online. It seemed like it worked...no more shredded sod clumps, or at least not as many. But the first time I wandered into the yard and was sprayed, I knew I needed to make it better.

For the first iteration, I connected the sprinkler to one of my summer irrigation valves. This wasn't a good idea. Those valves are controlled by solenoids and are not intended to be open 12+ hours at a time.

For the second iteration, I bought a 12VDC ball valve, and set up a basic timer ESP32 app. This worked OK, but even when the valve was closed, there was enough pressure in the hose to douse me once.

For the third iteration, I swapped out the 3x1.5v lithium batteries in the sprinkler for a hard-wired connection to the ESP32. The sprinkler works fine on the ESP32 5v power supply. Now when it's off, it's really off. I also modified the interface code to configure schedules and made it a bit prettier. The sprinkler is powered through an INA219 current sensor, so I can check the logs to see how many times it triggered and know how active the vermin were last night.

Incidentally, since the ESP32 is sitting beneath the deck, it shares functionality with the ultrasonic water sensor sitting above my rainwater tanks. This is awkward, since ESP32 MDNS cannot serve two hostnames from the same IP. I'm working on a fork of the library to correct this.
