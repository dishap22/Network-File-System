naming_server:
	gcc naming_server2.c data.c -o naming_server -lpthread

client:
	gcc client.c -o client

storage_server:
	gcc storage_server.c data.c -o storage_server -lpthread