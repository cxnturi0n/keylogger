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
        <li><a href="Idea#">General idea</a></li>
        <li><a href="#Socket">Socket or regular file</a></li>
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

