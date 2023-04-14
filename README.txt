Just a simple chat server written in c.

Can support up to 32 users simultaneously.

Steps to run:

  make all;

  ./chatServer portNumber

  *In another terminal*

  nc localHost portNumber

List of commands:

  register:USER
  
  list

  getMessage

  message:FROM_USER:TO_USER:CHAT_MESSAGE

  quit
