// *************************************************** //
// This project is based off this sample found on MSDN - 
// https://code.msdn.microsoft.com/windowsapps/Service-Bus-Event-Hub-45f43fc3
// 
// License - Apache 2.0
// 
// Author - Jason Fox
//
// The modifications to this project are designed to connect a microcontroller board(like the Intel Galileo) via a socket connection.
// A predetermined, ordered set of values is read from the socket, device id, voltage and temperature, and then send to an Azure Event Hub.
// *************************************************** //

namespace Sender
{
    using System.Configuration;
    using System;
    using System.Net.Sockets;
    using System.Text;
    using System.Threading.Tasks;
    using Microsoft.ServiceBus;
    using Microsoft.ServiceBus.Messaging;
    using System.Net;

    class Program
    {
        static string eventHubName;
        const string DEFAULT_SERVER = "169.254.79.76";
        const int DEFAULT_PORT = 27015; 
        static int defaultBufferLength = 512;

        static void Main(string[] args)
        {
            if (args.Length < 1)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine("Incorrect number of arguments. Expected: Sender <eventhubname>.");
                Console.ResetColor();
                Console.WriteLine("Press Ctrl-C or Enter to exit.");
                Console.ReadLine();
                return;
            }
            else
            {
                eventHubName = args[0];
                Console.WriteLine("EventHub name: " + eventHubName);
            }

            Console.WriteLine("Press Ctrl-C to stop the sender process");
            Console.WriteLine("Press Enter to start now");
            Console.ReadLine();
            if (CreateEventHub())
            {
                GetDataFromSocketAndSendMessage().Wait();
            }
        }

        static async Task GetDataFromSocketAndSendMessage()
        {
            var eventHubConnectionString = GetEventHubConnectionString();
            var eventHubClient = EventHubClient.CreateFromConnectionString(eventHubConnectionString, eventHubName);

            DnsEndPoint ep = new DnsEndPoint("mygalileo", DEFAULT_PORT);

            var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            socket.Connect(ep);

            while (true)
            {
                try
                {
                    
                    if(socket.Connected)
                    {
                        byte[] buffer = new byte[defaultBufferLength];
                        var bytesReceived = socket.Receive(buffer);

                        var dataStr = Encoding.UTF8.GetString(buffer);
                        dataStr = dataStr.Substring(0, dataStr.IndexOf("\n"));
                        string[] dataSplit = dataStr.Split(';');

                        // setup our data object - we obviously expect the string to come across the socket in a specific order
                        Data data = new Data()
                        {
                            DeviceID = Int32.Parse(dataSplit[0]),
                            Voltage = Int32.Parse(dataSplit[1]),
                            Temperature = float.Parse(dataSplit[2]),
                            TimeStamp = DateTime.Now
                        };

                        // convert the data into Json for portability
                        var message = Newtonsoft.Json.JsonConvert.SerializeObject(data);

                        EventData eventData = new EventData(Encoding.UTF8.GetBytes(message))
                        {
                            PartitionKey = data.DeviceID.ToString()// set the partition key for data organization 
                        };

                        Console.WriteLine("{0} > Sending message: {1}", DateTime.Now.ToString(), message);

                        await eventHubClient.SendAsync(eventData);
                    }

                }
                catch (Exception exception)
                {
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine("{0} > Exception: {1}", DateTime.Now.ToString(), exception.Message);
                    Console.ResetColor();
                }

                // we send it every 5 seconds for demo purposes
                await Task.Delay(5000);
            }       
        }

        static bool CreateEventHub()
        {
            try
            {
                var manager = NamespaceManager.CreateFromConnectionString(GetEventHubConnectionString());

                // Create the Event Hub
                Console.WriteLine("Creating Event Hub...");
                manager.CreateEventHubIfNotExistsAsync(eventHubName).Wait();
                Console.WriteLine("Event Hub is created...");
                return true;
            }
            catch (AggregateException agexp)
            {
                Console.WriteLine(agexp.Flatten());
                return false;
            }
        }
        
        static string GetEventHubConnectionString()
        {
            var connectionString = ConfigurationManager.AppSettings["Microsoft.ServiceBus.ConnectionString"];
            if (string.IsNullOrEmpty(connectionString))
            {
                Console.WriteLine("Did not find Service Bus connections string in appsettings (app.config)");
                return string.Empty;
            }
            try
            {
                var builder = new ServiceBusConnectionStringBuilder(connectionString);
                builder.TransportType = Microsoft.ServiceBus.Messaging.TransportType.Amqp;
                return builder.ToString();
            }
            catch (Exception exception)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(exception.Message);
                Console.ResetColor();
            }

            return null;
        }
    }
}
