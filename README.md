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
        <li><a href="#Sending">Sending events</a></li>
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

The function <code>int findKeyboardDevice(char *dir_path)</code> has the task of exploring devices and subdirectories(by calling itself recursively if current file is a directory) and returns the first keyboard device it finds, if any. How do we actually check if a character device is actually a keyboard one? We can use the event API (EVIOC* functions), which will allow us to query the capabilities and characteristics of an input device.
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

<H4 id="Sending"> Sending events </H4>
