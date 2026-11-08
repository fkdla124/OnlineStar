
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <chrono>
#include <random>
#include <string>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace std::chrono;

// ============================================================
// Settings
// ============================================================

constexpr int SOCKET_COUNT = 300;
constexpr int SERVER_PORT_DEFAULT = 3000;

constexpr int SCREEN_ROW = 23;
constexpr int SCREEN_COL = 80;

constexpr int MSG_SIZE = 16;
constexpr int RECV_BUFSIZE = 4096;

// ============================================================
// Message Protocol
// ============================================================

#pragma pack(push, 1)

struct MsgAllocID
{
    int type;
    int id;
    int dummy1;
    int dummy2;
};

struct MsgCreateStar
{
    int type;
    int id;
    int col;
    int row;
};

struct MsgDeleteStar
{
    int type;
    int id;
    int dummy1;
    int dummy2;
};

struct MsgMoveStar
{
    int type;
    int id;
    int col;
    int row;
};

#pragma pack(pop)

enum class MsgType
{
    ALLOCID = 0,
    CREATESTAR = 1,
    DELETESTAR = 2,
    MOVESTAR = 3
};

// ============================================================
// Socket State
// ============================================================

enum class State
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

// ============================================================
// Dummy Client
// ============================================================

struct DummySocket
{
    SOCKET sock = INVALID_SOCKET;

    State state = State::DISCONNECTED;

    // Connection Timing
    steady_clock::time_point nextAttempt;
    steady_clock::time_point disconnectTime;

    // Statistics
    int connectCount = 0;
    int disconnectCount = 0;

    // Star Data
    int id = -1;
    int row = 11;
    int col = 40;

    bool hasId = false;

    // Random Movement
    int direction = -1;

    steady_clock::time_point directionEnd;
    steady_clock::time_point nextMove;

    // Receive Buffer
    char recvBuffer[RECV_BUFSIZE]{};
    int recvBytes = 0;
};

array<DummySocket, SOCKET_COUNT> clients;

// ============================================================
// Network Data
// ============================================================

SOCKADDR_IN serverAddr{};

mt19937 rng(random_device{}());

// ============================================================
// Random
// ============================================================

int Random(int min, int max)
{
    return uniform_int_distribution<int>(min, max)(rng);
}

// ============================================================
// Retry
// ============================================================

void ScheduleRetry(int index)
{
    auto& client = clients[index];

    if (client.sock != INVALID_SOCKET)
    {
        closesocket(client.sock);
        client.sock = INVALID_SOCKET;
    }

    client.state = State::DISCONNECTED;

    client.id = -1;
    client.hasId = false;

    client.recvBytes = 0;
    client.direction = -1;

    client.nextAttempt =
        steady_clock::now() +
        milliseconds(Random(500, 2000));
}

// ============================================================
// Disconnect
// ============================================================

void CloseClient(int index)
{
    auto& client = clients[index];

    if (client.state == State::DISCONNECTED)
        return;

    cout << "[DISCONNECT] Socket " << index
        << " | ID: " << client.id
        << endl;

    client.disconnectCount++;

    ScheduleRetry(index);
}

// ============================================================
// Connection Success
// ============================================================

void OnConnectSuccess(int index)
{
    auto& client = clients[index];

    client.state = State::CONNECTED;

    client.disconnectTime =
        steady_clock::now() +
        milliseconds(Random(2000, 5000));

    cout << "[CONNECTED] Socket " << index
        << " | Total: " << client.connectCount
        << endl;
}

// ============================================================
// Connect
// ============================================================

void StartConnect(int index)
{
    auto& client = clients[index];

    client.sock = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (client.sock == INVALID_SOCKET)
    {
        cout << "[SOCKET ERROR] "
            << WSAGetLastError() << endl;

        ScheduleRetry(index);
        return;
    }

    // Nonblocking
    u_long nonBlocking = 1;

    if (ioctlsocket(
        client.sock,
        FIONBIO,
        &nonBlocking) == SOCKET_ERROR)
    {
        cout << "[IOCTL ERROR] "
            << WSAGetLastError() << endl;

        ScheduleRetry(index);
        return;
    }

    client.connectCount++;

    int result = connect(
        client.sock,
        (SOCKADDR*)&serverAddr,
        sizeof(serverAddr)
    );

    if (result == 0)
    {
        OnConnectSuccess(index);
        return;
    }

    int error = WSAGetLastError();

    if (error == WSAEWOULDBLOCK ||
        error == WSAEINPROGRESS ||
        error == WSAEALREADY)
    {
        client.state = State::CONNECTING;

        cout << "[CONNECTING] Socket "
            << index << endl;

        return;
    }

    cout << "[CONNECT ERROR] Socket "
        << index << " Error: "
        << error << endl;

    ScheduleRetry(index);
}

// ============================================================
// Random Star Movement
// ============================================================

// Direction
// 0: UP
// 1: DOWN
// 2: LEFT
// 3: RIGHT

void MoveStar(int index)
{
    auto& client = clients[index];

    if (!client.hasId)
        return;

    auto now = steady_clock::now();

    if (now < client.nextMove)
        return;

    // Choose a new direction
    if (client.direction == -1 ||
        now >= client.directionEnd)
    {
        client.direction = Random(0, 3);

        client.directionEnd =
            now + milliseconds(Random(400, 1200));
    }

    int newCol = client.col;
    int newRow = client.row;

    switch (client.direction)
    {
        case 0:
            newRow--;
            break;

        case 1:
            newRow++;
            break;

        case 2:
            newCol--;
            break;

        case 3:
            newCol++;
            break;
    }

    // Screen Boundary
    if (newCol < 0 || newCol >= SCREEN_COL ||
        newRow < 0 || newRow >= SCREEN_ROW)
    {
        client.direction = Random(0, 3);

        client.directionEnd =
            now + milliseconds(Random(400, 1200));

        client.nextMove =
            now + milliseconds(Random(100, 200));

        return;
    }

    // Create Move Message
    MsgMoveStar msg{};

    msg.type = (int)MsgType::MOVESTAR;
    msg.id = client.id;
    msg.col = newCol;
    msg.row = newRow;

    // Send
    int result = send(
        client.sock,
        (char*)&msg,
        sizeof(msg),
        0
    );

    if (result == SOCKET_ERROR)
    {
        int error = WSAGetLastError();

        if (error != WSAEWOULDBLOCK)
        {
            cout << "[MOVE ERROR] Socket "
                << index << " Error: "
                << error << endl;

            CloseClient(index);
        }

        return;
    }

    // Partial Send
    if (result != sizeof(msg))
    {
        cout << "[PARTIAL SEND] Socket "
            << index << endl;

        CloseClient(index);
        return;
    }

    client.col = newCol;
    client.row = newRow;

    // Next movement: 100~200ms
    client.nextMove =
        now + milliseconds(Random(100, 200));
}

// ============================================================
// Receive Message
// ============================================================

void ProcessReceive(int index)
{
    auto& client = clients[index];

    char tempBuffer[1024];

    int recvSize = recv(
        client.sock,
        tempBuffer,
        sizeof(tempBuffer),
        0
    );

    if (recvSize == 0)
    {
        cout << "[SERVER CLOSED] Socket "
            << index << endl;

        CloseClient(index);
        return;
    }

    if (recvSize == SOCKET_ERROR)
    {
        int error = WSAGetLastError();

        if (error != WSAEWOULDBLOCK)
        {
            cout << "[RECV ERROR] Socket "
                << index << " Error: "
                << error << endl;

            CloseClient(index);
        }

        return;
    }

    // Buffer Overflow Check
    if (client.recvBytes + recvSize >
        RECV_BUFSIZE)
    {
        cout << "[RECV BUFFER OVERFLOW] Socket "
            << index << endl;

        CloseClient(index);
        return;
    }

    // Append Received Data
    memcpy(
        client.recvBuffer + client.recvBytes,
        tempBuffer,
        recvSize
    );

    client.recvBytes += recvSize;

    // Parse 16-byte Messages
    while (client.recvBytes >= MSG_SIZE)
    {
        int type = -1;

        memcpy(
            &type,
            client.recvBuffer,
            sizeof(int)
        );

        switch ((MsgType)type)
        {
            case MsgType::ALLOCID:
            {
                MsgAllocID msg{};

                memcpy(
                    &msg,
                    client.recvBuffer,
                    sizeof(msg)
                );

                client.id = msg.id;
                client.hasId = true;

                // Initial Star Position
                client.col = Random(20, 60);
                client.row = Random(5, 18);

                // Start Random Movement
                client.direction = -1;
                client.nextMove = steady_clock::now();

                cout << "[ALLOC ID] Socket "
                    << index
                    << " | ID: " << client.id
                    << endl;

                break;
            }

            case MsgType::CREATESTAR:
            case MsgType::DELETESTAR:
            case MsgType::MOVESTAR:
            {
                // Not needed for dummy client
                break;
            }

            default:
            {
                cout << "[UNKNOWN MESSAGE] Socket "
                    << index
                    << " | Type: " << type
                    << endl;

                // Fixed 16-byte protocol assumption
                break;
            }
        }

        // Remove processed message
        memmove(
            client.recvBuffer,
            client.recvBuffer + MSG_SIZE,
            client.recvBytes - MSG_SIZE
        );

        client.recvBytes -= MSG_SIZE;
    }
}

// ============================================================
// Print Status
// ============================================================

void PrintStatus()
{
    int disconnected = 0;
    int connecting = 0;
    int connected = 0;

    for (const auto& client : clients)
    {
        switch (client.state)
        {
            case State::DISCONNECTED:
                disconnected++;
                break;

            case State::CONNECTING:
                connecting++;
                break;

            case State::CONNECTED:
                connected++;
                break;
        }
    }

    cout << "\n====================================\n";

    cout << "[STATUS] "
        << "Connected: " << connected
        << " | Connecting: " << connecting
        << " | Disconnected: " << disconnected
        << endl;

    cout << "====================================\n";

    for (int i = 0; i < SOCKET_COUNT; i++)
    {
        const auto& client = clients[i];

        cout << "Socket " << i
            << " | ";

        switch (client.state)
        {
            case State::DISCONNECTED:
                cout << "DISCONNECTED";
                break;

            case State::CONNECTING:
                cout << "CONNECTING";
                break;

            case State::CONNECTED:
                cout << "CONNECTED";
                break;
        }

        if (client.state == State::CONNECTED)
        {
            cout << " | ID: " << client.id
                << " | Pos: ("
                << client.col << ", "
                << client.row << ")";
        }

        cout << endl;
    }

    cout << "====================================\n";
}

// ============================================================
// Main Process
// ============================================================

void Process()
{
    auto lastPrint = steady_clock::now();

    while (true)
    {
        auto now = steady_clock::now();

        // ----------------------------------------------------
        // 1. Start Connections
        // ----------------------------------------------------

        for (int i = 0; i < SOCKET_COUNT; i++)
        {
            auto& client = clients[i];

            if (client.state == State::DISCONNECTED &&
                now >= client.nextAttempt)
            {
                StartConnect(i);
            }
        }

        // ----------------------------------------------------
        // 2. Select
        // ----------------------------------------------------

        fd_set readSet;
        fd_set writeSet;
        fd_set exceptSet;

        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);

        for (auto& client : clients)
        {
            if (client.sock == INVALID_SOCKET)
                continue;

            if (client.state == State::CONNECTING)
            {
                FD_SET(client.sock, &writeSet);
                FD_SET(client.sock, &exceptSet);
            }
            else if (client.state == State::CONNECTED)
            {
                FD_SET(client.sock, &readSet);
            }
        }

        timeval timeout{};

        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;

        int result = select(
            0,
            &readSet,
            &writeSet,
            &exceptSet,
            &timeout
        );

        if (result == SOCKET_ERROR)
        {
            cout << "[SELECT ERROR] "
                << WSAGetLastError() << endl;

            break;
        }

        // ----------------------------------------------------
        // 3. Connecting State
        // ----------------------------------------------------

        for (int i = 0; i < SOCKET_COUNT; i++)
        {
            auto& client = clients[i];

            if (client.state != State::CONNECTING)
                continue;

            if (FD_ISSET(client.sock, &writeSet) ||
                FD_ISSET(client.sock, &exceptSet))
            {
                int error = 0;
                int len = sizeof(error);

                if (getsockopt(
                    client.sock,
                    SOL_SOCKET,
                    SO_ERROR,
                    (char*)&error,
                    &len) == SOCKET_ERROR)
                {
                    error = WSAGetLastError();
                }

                if (error == 0)
                {
                    OnConnectSuccess(i);
                }
                else
                {
                    cout << "[CONNECT FAILED] Socket "
                        << i << " Error: "
                        << error << endl;

                    ScheduleRetry(i);
                }
            }
        }

        // ----------------------------------------------------
        // 4. Receive
        // ----------------------------------------------------

        for (int i = 0; i < SOCKET_COUNT; i++)
        {
            auto& client = clients[i];

            if (client.state != State::CONNECTED)
                continue;

            if (FD_ISSET(client.sock, &readSet))
            {
                ProcessReceive(i);
            }
        }

        // ----------------------------------------------------
        // 5. Random Star Movement
        // ----------------------------------------------------

        for (int i = 0; i < SOCKET_COUNT; i++)
        {
            auto& client = clients[i];

            if (client.state == State::CONNECTED &&
                client.hasId)
            {
                MoveStar(i);
            }
        }

        // ----------------------------------------------------
        // 6. Disconnect When Time Expires
        // ----------------------------------------------------

        now = steady_clock::now();

        for (int i = 0; i < SOCKET_COUNT; i++)
        {
            auto& client = clients[i];

            if (client.state == State::CONNECTED &&
                now >= client.disconnectTime)
            {
                CloseClient(i);
            }
        }

        // ----------------------------------------------------
        // 7. Print Status
        // ----------------------------------------------------

        if (steady_clock::now() - lastPrint >=
            milliseconds(1000))
        {
            PrintStatus();

            lastPrint = steady_clock::now();
        }
    }
}

// ============================================================
// Main
// ============================================================

int main()
{
    // WSAStartup
    WSADATA wsa{};

    int result = WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    );

    if (result != 0)
    {
        cout << "WSAStartup Error: "
            << result << endl;

        return 1;
    }

    // Server IP
    string ip;

    int port = SERVER_PORT_DEFAULT;

    cout << "Server IP: ";
    cin >> ip;

    cout << "Server Port: ";
    cin >> port;

    // Server Address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (InetPtonA(
        AF_INET,
        ip.c_str(),
        &serverAddr.sin_addr) != 1)
    {
        cout << "Invalid Server IP" << endl;

        WSACleanup();
        return 1;
    }

    // --------------------------------------------------------
    // Initialize 10 Sockets
    // --------------------------------------------------------

    auto now = steady_clock::now();

    for (int i = 0; i < SOCKET_COUNT; i++)
    {
        clients[i].state = State::DISCONNECTED;

        // Initial connection interval
        // Socket 0: immediately
        // Socket 1: 200ms later
        // Socket 2: 400ms later ...
        clients[i].nextAttempt =
            now + milliseconds(i * 200);
    }

    cout << "\n====================================\n";
    cout << "       DUMMY CLIENT STARTED\n";
    cout << "====================================\n";

    cout << "Socket Count: "
        << SOCKET_COUNT << endl;

    cout << "Random Movement: ON" << endl;
    cout << "Auto Reconnect: ON" << endl;
    cout << "Press Ctrl+C to exit." << endl;

    cout << "====================================\n";

    // --------------------------------------------------------
    // Run
    // --------------------------------------------------------

    Process();

    // --------------------------------------------------------
    // Cleanup
    // --------------------------------------------------------

    for (auto& client : clients)
    {
        if (client.sock != INVALID_SOCKET)
        {
            closesocket(client.sock);
            client.sock = INVALID_SOCKET;
        }
    }

    WSACleanup();

    return 0;
}
