# Keylogger-C-Unix

<H4>A simple daemon keylogger written in C on Unix platform, that sends keyboard press events to a server or stores them locally.</H5>

<H2> Table of contents </H2>
<ul>
  <li><a href="#Usage">Usage</a></li>
  <ul>
    <li><a href="#Compiling">Compiling</a></li>
    <li><a href="#Arguments">Command line arguments</a></li>
  </ul>
  <li><a href="#How">How does it work?</a></li>
  <ul>
    <li><a href="#Keylogger">Keylogger</a></li>
      <ul>
        <li><a href="#Idea">General idea</a></li>
        <li><a href="#Finding">Finding keyboard device</a></li>
        <li><a href="#Reading">Reading events</a></li>
        <li><a href="#Signals">Signals handling</a></li>
      </ul>
    <li><a href="#Daemon">Daemon process</a></li>
    <li><a href="#Server">Server</a></li>
  </ul>
  <li><a href="#References">References</a></li>
  <li><a href="#Disclaimer">Disclaimer</a></li>
</ul>

<H2 id="Usage"> Usage </H2>

<H3 id="Compiling"> Compiling </H3>

In order to compile the program, run this command: 
```

gcc main.c keylogger.c daemon.c -o daemon-keylogger

```

<H3 id="Arguments"> Command line arguments </H3>

You can invoke the executable in two possible ways:
<ul>
  <li>&lt<b>executable_path</b>&gt &lt<b>host</b>&gt &lt<b>port</b>&gt &lt<b>is_single_instance</b>&gt</li>
  <li>&lt<b>executable_path</b>&gt &lt<b>file_path</b>&gt &lt<b>is_single_instance</b>&gt</li>
</ul>
Where:
<ul>
  <li><b>executable_path</b>: path of the executable</li>
  <li><b>host</b> and <b>port</b>: respectively the hostname or ip address and the port of the server you want to send the events to</li>
  <li><b>file_path</b>: path of the regular file you want to store events into</li> 
  <li><b>is_single_instance</b>: 1 if you only want an istance of the daemon running, 0 otherwise</li>
</ul>
Running examples: <code>./daemon-keylogger 127.0.0.1 12345 0</code> or <code>./daemon-keylogger file_out.txt 1</code>.


<H2 id="How"> How does it work? </H2>

<H3 id="Keylogger"> Keylogger </H3>

<H4 id="Idea"> General idea </H4>
First of all, you have to specify where to send the events, to a server or locally in a regular file, and you also have to choose between allowing multiple instances of the daemon run simultaneously (for example, you may want to send events to multiple servers) or having just one copy of the daemon running. However, i added this option just for practice rather than for a particular useful purpose :stuck_out_tongue_closed_eyes:. If the daemon is single instance, before starting the keylogger module, we have to check if another copy of this daemon is already running, in such case the program simply exits. After that, the keyboard device driver is looked for, the socket or the file is opened and the keylogging phase begins. The program consists of 5 main parts:
<ol>
  <li>Daemonize process: In this phase, the process is converted into a daemon, which is a process that doesn't have a controlling terminal, for this reason it runs in the background. </li>
  <li>Guarantee single instance: If you specified to have a single instance daemon, the program has to assure that another copy is not already running, in that case a lock of a particular file is obtained.</li>
  <li>Find keyboard device driver: It looks for the keyboard device driver into <b>/dev/input/</b>.</li>
  <li>Open connection or file: If you choose to send events to a server, a connection with it must be established, if you choose to save events locally, the file has to be opened. </li>
  <li>Keylog: Reads keypress events from the keyboard device driver and sends them to server or file. </li>
</ol>

<H4 id="Finding"> Finding keyboard device </H4>
The event interface exposes the raw events to userspace through a collection of character device nodes, one character device node per logical input device(keyboard, mouse, joystick, power buttons, ..). Those device files can be found into <b>/dev/input/</b>.

The function <code>int findKeyboardDevice(char \*dir_path)</code> has the task of exploring devices and subdirectories(by calling itself recursively if current file is a directory) and returns the first keyboard device it finds, if any. How do we actually check if a character device is actually a keyboard one? We can use the event API (EVIOC* functions), which will allow us to query the capabilities and characteristics of an input device.
Take a look at a function that uses the linux event API to check whether or not an input device is a keyboard device:

```c
   
int isKeyboardDevice(char *path, int *keyboard_device)
{
    int32_t events_bitmask = 0;
    int32_t keys_bitmask = 0;
    int32_t keys = KEY_Q | KEY_A | KEY_Z | KEY_1 | KEY_9;
    int fd;

    fd = open(path, O_RDONLY);

    if (fd < 0)
        syslog(LOG_ERR, "Couldn't open %s: %s", path, strerror(errno));

    if (ioctl(fd, EVIOCGBIT(0, sizeof(events_bitmask)), &events_bitmask) >= 0)
    {
        if ((events_bitmask & EV_KEY) == EV_KEY) 
        {
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys_bitmask)), &keys_bitmask) >= 0)
                if ((keys & keys_bitmask) == keys)
                {
                    *keyboard_device = fd;
                    return 1;
                }
        }
    }
    close(fd);
    return 0;
}

```

The first ioctl call allows us to get the event bits, that is, a bitmask expressing all of the capabilities or features supported by the device, it can tell us if, for example, the device has keys or buttons or if it a mouse.
If EV_KEY bit is set then we have found a device that has keys or buttons, but we cannot yet be sure that it is a keyboard! A power button will have this bit set but it is not obviously a keyboard. 
However, EVIOCGBIT permits us to make more precise queries about the specific device features, in our case, the second ioctl call, will allow us to get to know if the device supports "q", "a", "z", "1" and "9", if yes, we can almost be sure that it is a keyboard device.

<H4 id="Reading"> Reading events </H4>
Retrieving events from a device requires a standard character device “read" function. Each time you read from an event device you will get a number of events. Every event consists of a struct input_event. If you wish to know more about input_event fields, give a look at https://www.kernel.org/doc/Documentation/input/event-codes.txt.

```c

struct input_event {
      struct timeval time;
      unsigned short type;
      unsigned short code;
      unsigned int value; 
      };

```

When trying to read for the keyboard device you may not get exactly what you would expect, that is, a unique event containing the code of the key you have just pressed. This happens because, generally, a single hardware event is mapped into multiple "software" input events. This is what happens if i try to read from the keyboard device after pressing the letter "a":

![image](https://user-images.githubusercontent.com/75443422/177054820-ff481134-024d-46e1-ba5c-0a404d88f2ac.png)

As you can see, a single key press has generated six input events. Let us take a look at each of those events events:
<ol>
<li>Type = 4 indicates an EV_MSC event, which according to the documentation is used to describe miscellaneous input data that do not fit into other types. From what my understanding is, it returns in the "value" field the device specific scan code, so, we are not really interested in it, because we could get wrong key codes if user remaps the keys. It is not completely useless though, it could be used to recognize which specific physical buttons are being pressed</li>
<li>This is the event we are mostly interested in. It has type = EV_KEY, which tells us that a key has either been pressed, released or repeated,  value = 1 tells us that a key has been pressed and code = "30" represents the KEY_A key, which is in fact the key i pressed.</li>
<li>Type = 0 indicates an EV_SYN event, which is simply used to separate different hardware events.</li>
<li>The other three events generated are almost the same to the first three, they are associated to the hardware event of "releasing a key". If you take a look at the fifth event, you can see that we have an EV_KEY event with value = 0 that represents a key release, in this case, of the letter "a".</li>







