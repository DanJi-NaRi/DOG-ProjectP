using System;
using System.Linq;
using Gauntlet;

namespace UnrealGame
{
    public class ProjectPLoginLobbyTest : UnrealTestNode<UnrealTestConfig>
    {
        private const int ClientCount = 3;
        private const int DefaultMaxDurationSeconds = 300;
        private const string CredentialsFileParamName = "GauntletCredentialsFile";
        private const string SuccessMarkerFormat = "GAUNTLET_LOGIN_LOBBY_CLIENT_{0}_SUCCESS";
        private const string FailureMarkerFormat = "GAUNTLET_LOGIN_LOBBY_CLIENT_{0}_FAILURE";

        private readonly bool[] clientSucceeded = new bool[ClientCount];
        private readonly UnrealLogStreamParser[] clientLogParsers = new UnrealLogStreamParser[ClientCount];

        public ProjectPLoginLobbyTest(UnrealTestContext inContext)
            : base(inContext)
        {
        }

        public override UnrealTestConfig GetConfiguration()
        {
            UnrealTestConfig config = base.GetConfiguration();
            string credentialsFile = Context.TestParams.ParseValue(CredentialsFileParamName, string.Empty);

            if (string.IsNullOrWhiteSpace(credentialsFile))
            {
                throw new TestException("Missing required Gauntlet credentials file. Pass -GauntletCredentialsFile=<path>.");
            }

            UnrealTestRole[] clients = config.RequireRoles(UnrealTargetRole.Client, ClientCount).ToArray();
            ConfigureClient(clients[0], 1, "DuplicateLogin", credentialsFile);
            ConfigureClient(clients[1], 2, "WrongPassword", credentialsFile);
            ConfigureClient(clients[2], 3, "WrongID", credentialsFile);

            config.MaxDuration = Context.TestParams.ParseValue("MaxDuration", DefaultMaxDurationSeconds);
            return config;
        }

        public override void TickTest()
        {
            if (GetTestStatus() == TestStatus.Complete)
            {
                return;
            }

            if (TestInstance?.ClientApps != null && TestInstance.ClientApps.Length >= ClientCount)
            {
                for (int clientIndex = 1; clientIndex <= ClientCount; clientIndex++)
                {
                    IAppInstance clientApp = TestInstance.ClientApps[clientIndex - 1];
                    UnrealLogStreamParser logParser = GetClientLogParser(clientIndex, clientApp);
                    logParser.ReadStream();

                    if (logParser.GetLogLinesContaining(string.Format(FailureMarkerFormat, clientIndex)).Any())
                    {
                        FailTest(string.Format("Client {0} reported login lobby failure marker.", clientIndex));
                        return;
                    }

                    if (!clientSucceeded[clientIndex - 1] && logParser.GetLogLinesContaining(string.Format(SuccessMarkerFormat, clientIndex)).Any())
                    {
                        clientSucceeded[clientIndex - 1] = true;
                        Log.Info("Client {0} completed login lobby flow.", clientIndex);
                    }

                    if (clientApp.HasExited && !clientSucceeded[clientIndex - 1])
                    {
                        FailTest(string.Format("Client {0} exited before reporting login lobby success.", clientIndex));
                        return;
                    }
                }

                if (clientSucceeded.All(value => value))
                {
                    Log.Info("All three login lobby clients completed.");
                    MarkTestComplete();
                    SetUnrealTestResult(TestResult.Passed);
                    return;
                }
            }

            base.TickTest();
        }

        private static void ConfigureClient(UnrealTestRole client, int clientIndex, string failureCase, string credentialsFile)
        {
            client.CommandLineParams.Add("GauntletLoginLobbyTest");
            client.CommandLineParams.Add("GauntletClientIndex", clientIndex);
            client.CommandLineParams.Add("GauntletLoginFailureCase", failureCase);
            client.CommandLineParams.Add("GauntletCredentialsFile", credentialsFile);
            client.CommandLineParams.Add("windowed");
            client.CommandLineParams.Add("ResX", 640);
            client.CommandLineParams.Add("ResY", 360);
            client.CommandLineParams.Add("unattended");
            client.CommandLineParams.Add("log");
        }

        private UnrealLogStreamParser GetClientLogParser(int clientIndex, IAppInstance clientApp)
        {
            int arrayIndex = clientIndex - 1;
            if (clientLogParsers[arrayIndex] == null)
            {
                clientLogParsers[arrayIndex] = new UnrealLogStreamParser(clientApp.GetLogBufferReader());
            }

            return clientLogParsers[arrayIndex];
        }

        private void FailTest(string message)
        {
            ReportError(message);
            MarkTestComplete();
            SetUnrealTestResult(TestResult.Failed);
        }
    }
}
