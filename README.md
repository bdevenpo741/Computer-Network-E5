The checksum.cpp/checksum.h is a checker that ensures the server and client receive the correct data. 
The Client code the put function works at connecting to server and sending the testfile. 
In order for it to work on your computer you will need to change localhost to be you compter IP.
The biggest issue right now is having the sever not able to recognize the command it receives from the client. 
The server receives 'put testfile.txt' but doesn't understand the command and filename. The get function has a lot of work too. 
I have only been able to somewhat get the put function to work on the client side but the server still needs work. Feel free to change any of the code. 
