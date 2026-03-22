//    SoulFu - 3D Rogue-like dungeon crawler
//    Copyright (C) 2007 Aaron Bishop
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//    web:   http://www.aaronbishopgames.com
//    email: aaron@aaronbishopgames.com

// <ZZ> This file contains functions to handle networking
//      network_blah			- Blah

#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#define ALLOW_LOCAL_PACKETS


#define UDP_PORT 17859          // Orangeville, PA
#define TCPIP_PORT UDP_PORT
#ifdef LIL_ENDIAN
    #define LOCALHOST ((127) | (0<<8) | (0<<16) | (1<<24))
#else
    #define LOCALHOST ((127<<24) | (0<<16) | (0<<8) | (1))
#endif
#define MAX_REMOTE       1024   // Maximum number of network'd computers...





unsigned char network_on;
unsigned char* netlist = NULL;


IPaddress       local_address;
IPaddress       main_server_address;
unsigned char   main_server_on = FALSE;
#define MAIN_SERVER_NAME "soulfu.untier.eu"


UDPsocket       remote_socket;
IPaddress	    remote_address[MAX_REMOTE];
unsigned short	remote_room_number[MAX_REMOTE];
unsigned char	remote_is_neighbor[MAX_REMOTE];
unsigned char	remote_on[MAX_REMOTE];
unsigned char	remote_ready[MAX_REMOTE];
unsigned char	remote_relay[MAX_REMOTE];        // TRUE if traffic routes through server relay
unsigned int    remote_heartbeat[MAX_REMOTE];   // SDL_GetTicks() of last packet from this remote
unsigned short  num_remote = 0;
#define PEER_HEARTBEAT_INTERVAL_MS  3000    // Send heartbeat every 3 seconds
#define PEER_TIMEOUT_MS             15000   // Disconnect after 15 seconds of silence
#define PACKET_TYPE_PEER_HEARTBEAT  9       // Peer-to-peer keepalive (distinct from server heartbeat 29)
unsigned int    peer_heartbeat_last = 0;        // SDL_GetTicks() of last heartbeat sent


// LAN broadcast discovery
#define LAN_BROADCAST_PORT 17860
#define LAN_BROADCAST_INTERVAL 120      // Frames between broadcasts (~2 seconds at 60fps)
UDPsocket       lan_broadcast_socket;
unsigned char   lan_hosting = FALSE;
unsigned short  lan_broadcast_timer = 0;
#define MAX_LAN_GAMES 16
IPaddress       lan_game_address[MAX_LAN_GAMES];
unsigned short  lan_game_players[MAX_LAN_GAMES];
unsigned char   lan_game_on[MAX_LAN_GAMES];
unsigned short  lan_game_timeout[MAX_LAN_GAMES];
unsigned short  num_lan_games = 0;

// Join handshake state
unsigned char   join_state = 0;         // 0=idle, 1=sent request, 2=got reply, 3=finalizing, 4=connected
unsigned short  join_timer = 0;
IPaddress       join_target_address;

// UDP hole punch state (for NAT traversal via master server)
unsigned char   punch_active = FALSE;
unsigned int    punch_timer = 0;
unsigned char   punch_retries = 0;
unsigned char   punch_role = 0;         // 0=joiner, 1=host
IPaddress       punch_target;           // Peer's public IP:port as seen by server
unsigned char   punch_continent, punch_direction, punch_letter, punch_pw_ok;
#define PUNCH_INTERVAL_MS   500         // Send punch every 0.5 seconds
#define PUNCH_MAX_RETRIES   20          // Try for ~10 seconds




#define MAX_PACKET_SIZE 8192
#define PACKET_HEADER_SIZE  3
#define PACKET_TYPE_CHAT                    0
#define PACKET_TYPE_I_AM_HERE               1
#define PACKET_TYPE_ROOM_UPDATE             2
#define PACKET_TYPE_I_WANNA_PLAY            3
#define PACKET_TYPE_OKAY_YOU_CAN_PLAY       4
#define PACKET_TYPE_LAN_ANNOUNCE            5
#define PACKET_TYPE_LAN_QUERY               6
#define PACKET_TYPE_PLAYER_READY            7
#define PACKET_TYPE_START_GAME              8

// ServerFu master server packet types (unencrypted - server has no random_table)
#define PACKET_TYPE_REQUEST_SHARD_LIST      10
#define PACKET_TYPE_REPLY_SHARD_LIST        11
#define PACKET_TYPE_REQUEST_PLAYER_COUNT    12
#define PACKET_TYPE_REPLY_PLAYER_COUNT      13
#define PACKET_TYPE_REPLY_VERSION_ERROR     14
#define PACKET_TYPE_REQUEST_JOIN            15
#define PACKET_TYPE_COMMAND_JOIN            16
#define PACKET_TYPE_REPLY_JOIN_OKAY         17
#define PACKET_TYPE_REPLY_ROGER             18
#define PACKET_TYPE_REQUEST_IP_LIST         19
#define PACKET_TYPE_REPLY_IP_LIST           20
#define PACKET_TYPE_REPORT_MACHINE_DOWN     28
#define PACKET_TYPE_HEARTBEAT               29
#define PACKET_TYPE_REPORT_POSITION         30
#define PACKET_TYPE_PUNCH_REQUEST           31
#define PACKET_TYPE_PUNCH_ACK               32
#define PACKET_TYPE_RELAY_REQUEST           33
#define PACKET_TYPE_RELAY_DATA              34

#define EXECUTABLE_VERSION_NUMBER   1
#define DATA_VERSION_NUMBER         1
#define PASSWORD_OKAY_VALUE         213

// Master server shard discovery state
#define MAX_SHARDS 26
unsigned int  shard_valid_flags = 0;
unsigned int  shard_ip[MAX_SHARDS];
unsigned int  my_public_ip = 0;
unsigned short server_player_count = 0;
unsigned short server_heartbeat_timer = 0;
#define SERVER_HEARTBEAT_INTERVAL 3600   // ~60 seconds at 60fps

#define NETWORK_ALL_REMOTES_IN_GAME             0
#define NETWORK_ALL_REMOTES_IN_ROOM             1
#define NETWORK_ALL_REMOTES_IN_NEARBY_ROOMS     2
#define NETWORK_SERVER_VIA_TCPIP                3

unsigned char packet_buffer[MAX_PACKET_SIZE];
unsigned short packet_length;
unsigned short packet_counter;
unsigned short packet_readpos;
unsigned char packet_seed;
unsigned char packet_checksum;


unsigned char global_version_error = FALSE;
unsigned short required_executable_version = 65535;
unsigned short required_data_version = 65535;


unsigned char  network_script_newly_spawned;
unsigned char  network_script_extra_data;
unsigned char  network_script_remote_index;
unsigned char  network_script_netlist_index;
unsigned short network_script_x;
unsigned short network_script_y;
unsigned char  network_script_z;
unsigned char  network_script_facing;
unsigned char  network_script_action;
unsigned char  network_script_team;
unsigned char  network_script_poison;
unsigned char  network_script_petrify;
unsigned char  network_script_alpha;
unsigned char  network_script_deflect;
unsigned char  network_script_haste;
unsigned char  network_script_other_enchant;
unsigned char  network_script_eqleft;
unsigned char  network_script_eqright;
unsigned char  network_script_eqcol01;
unsigned char  network_script_eqcol23;              // high-data only
unsigned char  network_script_eqspec1;              // high-data only
unsigned char  network_script_eqspec2;              // high-data only
unsigned char  network_script_eqhelm;               // high-data only
unsigned char  network_script_eqbody;               // high-data only
unsigned char  network_script_eqlegs;               // high-data only
unsigned char  network_script_class;                // high-data only
unsigned short network_script_mount_index;          // high-data only





//-----------------------------------------------------------------------------------------------
// Packet macros...
//-----------------------------------------------------------------------------------------------
#define packet_begin(type)                                                  \
{                                                                           \
    packet_length = PACKET_HEADER_SIZE;                                     \
    packet_buffer[0] = (unsigned char) type;                                \
    packet_buffer[1] = 0;                                                   \
    packet_buffer[2] = 0;                                                   \
}
// packet_buffer[0] is the packet type...
// packet_buffer[1] is the checksum
// packet_buffer[2] is the random seed


//-----------------------------------------------------------------------------------------------
#define packet_add_string(string)                                           \
{                                                                           \
    packet_counter = 0;                                                     \
    while(string[packet_counter] != 0)                                      \
    {                                                                       \
        packet_buffer[packet_length] = string[packet_counter];              \
        packet_length++;                                                    \
        packet_counter++;                                                   \
    }                                                                       \
    packet_buffer[packet_length] = 0;                                       \
    packet_length++;                                                        \
}

//-----------------------------------------------------------------------------------------------
#define packet_add_unsigned_int(number)                                     \
{                                                                           \
    packet_buffer[packet_length] = (unsigned char) ((number)>>24);          \
    packet_buffer[packet_length+1] = (unsigned char) ((number)>>16);        \
    packet_buffer[packet_length+2] = (unsigned char) ((number)>>8);         \
    packet_buffer[packet_length+3] = (unsigned char) (number);              \
    packet_length+=4;                                                       \
}

//-----------------------------------------------------------------------------------------------
#define packet_add_unsigned_short(number)                                   \
{                                                                           \
    packet_buffer[packet_length] = (unsigned char) (number>>8);             \
    packet_buffer[packet_length+1] = (unsigned char) number;                \
    packet_length+=2;                                                       \
}

//-----------------------------------------------------------------------------------------------
#define packet_add_unsigned_char(number)                                    \
{                                                                           \
    packet_buffer[packet_length] = (unsigned char) number;                  \
    packet_length++;                                                        \
}

//-----------------------------------------------------------------------------------------------
#define packet_read_string(string)                                          \
{                                                                           \
    packet_counter = 0;                                                     \
    while(packet_buffer[packet_readpos] != 0 && packet_readpos < MAX_PACKET_SIZE && packet_counter < 250)    \
    {                                                                       \
        string[packet_counter] = packet_buffer[packet_readpos];             \
        packet_counter++;                                                   \
        packet_readpos++;                                                   \
    }                                                                       \
    string[packet_counter] = 0;                                             \
    packet_readpos++;                                                       \
}

//-----------------------------------------------------------------------------------------------
#define packet_read_unsigned_int(to_set)                                    \
{                                                                           \
    to_set = packet_buffer[packet_readpos];                                 \
    to_set = to_set << 8;                                                   \
    to_set |= packet_buffer[packet_readpos+1];                              \
    to_set = to_set << 8;                                                   \
    to_set |= packet_buffer[packet_readpos+2];                              \
    to_set = to_set << 8;                                                   \
    to_set |= packet_buffer[packet_readpos+3];                              \
    packet_readpos+=4;                                                      \
}

//-----------------------------------------------------------------------------------------------
#define packet_read_unsigned_short(to_set)                                  \
{                                                                           \
    to_set = packet_buffer[packet_readpos];                                 \
    to_set = to_set << 8;                                                   \
    to_set |= packet_buffer[packet_readpos+1];                              \
    packet_readpos+=2;                                                      \
}

//-----------------------------------------------------------------------------------------------
#define packet_read_unsigned_char(to_set)                                   \
{                                                                           \
    to_set = packet_buffer[packet_readpos];                                 \
    packet_readpos++;                                                       \
}

//-----------------------------------------------------------------------------------------------
#define packet_encrypt()                                                    \
{                                                                           \
    packet_seed = random_number;                                            \
    packet_buffer[1] = packet_seed;                                         \
    packet_counter = PACKET_HEADER_SIZE;                                    \
    while(packet_counter < packet_length)                                   \
    {                                                                       \
        packet_buffer[packet_counter] += random_table[(packet_seed+2173-packet_counter)&and_random];         \
        packet_counter++;                                                   \
    }                                                                       \
}

//-----------------------------------------------------------------------------------------------
#define packet_decrypt()                                                    \
{                                                                           \
    packet_counter = PACKET_HEADER_SIZE;                                    \
    packet_seed = packet_buffer[1];                                         \
    while(packet_counter < packet_length)                                   \
    {                                                                       \
        packet_buffer[packet_counter] -= random_table[(packet_seed+2173-packet_counter)&and_random];         \
        packet_counter++;                                                   \
    }                                                                       \
}

//-----------------------------------------------------------------------------------------------
#define calculate_packet_checksum()                                         \
{                                                                           \
    packet_checksum = 0;                                                    \
    packet_counter = PACKET_HEADER_SIZE;                                    \
    while(packet_counter < packet_length)                                   \
    {                                                                       \
        packet_checksum += packet_buffer[packet_counter];                   \
        packet_counter++;                                                   \
    }                                                                       \
}

//-----------------------------------------------------------------------------------------------
#define packet_end()                                                        \
{                                                                           \
    calculate_packet_checksum();                                            \
    packet_buffer[2] = packet_checksum;                                     \
    packet_encrypt();                                                       \
}

//-----------------------------------------------------------------------------------------------
// Plain packet end - checksum only, no encryption (for master server packets)
#define packet_end_plain()                                                  \
{                                                                           \
    calculate_packet_checksum();                                            \
    packet_buffer[2] = packet_checksum;                                     \
    packet_buffer[1] = 0;                                                   \
}

//-----------------------------------------------------------------------------------------------
unsigned char packet_valid()
{
    calculate_packet_checksum();
    return (packet_buffer[2] == packet_checksum);
}

//-----------------------------------------------------------------------------------------------





//-----------------------------------------------------------------------------------------------
void network_clear_remote_list()
{
    // <ZZ> This function clears the list of other network players...
    unsigned short i;
    num_remote = 0;
    repeat(i, MAX_REMOTE)
    {
        remote_on[i] = FALSE;
        remote_ready[i] = FALSE;
        remote_relay[i] = FALSE;
    }
}

//-----------------------------------------------------------------------------------------------
unsigned char network_add_remote(unsigned char* remote_name)
{
    // <ZZ> This function adds a new network player by either address ("192.168.0.12") or
    //      name ("Frizzlesnitz")...  Returns TRUE if it worked (usually does)...
    unsigned short i;
    IPaddress temp_address;


    if(remote_name)
    {
        log_message("INFO:   Trying to add %s as a new remote...", remote_name);
        if(SDLNet_ResolveHost(&temp_address, remote_name, UDP_PORT) == 0)
        {
            log_message("INFO:   Found IP...  It's %d.%d.%d.%d", ((unsigned char*)&temp_address.host)[0], ((unsigned char*)&temp_address.host)[1], ((unsigned char*)&temp_address.host)[2], ((unsigned char*)&temp_address.host)[3]);
        }
        else
        {
            log_message("ERROR:  Couldn't find the IP address...  Oh, well...");
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }


    if(num_remote < MAX_REMOTE)
    {
        repeat(i, MAX_REMOTE)
        {
            if(remote_on[i])
            {
                if(remote_address[i].host == temp_address.host)
                {
                    log_message("ERROR:  That IP address is already used by remote %d...", i);
                    return FALSE;
                }
            }
        }


        repeat(i, MAX_REMOTE)
        {
            if(remote_on[i] == FALSE)
            {
                log_message("INFO:   Added new remote as remote number %d", i);
                remote_address[i].host = temp_address.host;
                remote_on[i] = TRUE;
                remote_room_number[i] = 65535;
                remote_is_neighbor[i] = FALSE;
                remote_heartbeat[i] = SDL_GetTicks();
                num_remote++;
                return TRUE;
            }
        }
    }
    log_message("ERROR:  Too many remotes all ready...  Oh, well...");
    return FALSE;
}

//-----------------------------------------------------------------------------------------------
unsigned char network_add_remote_ip_port(unsigned int host_ip, unsigned short port_net_order)
{
    // <ZZ> Adds a remote by IP address and port (for NAT traversal - port may differ from default)
    unsigned short i;

    if(num_remote < MAX_REMOTE)
    {
        // Check for duplicates
        repeat(i, MAX_REMOTE)
        {
            if(remote_on[i])
            {
                if(remote_address[i].host == host_ip)
                {
                    // Update port if it changed (NAT may reassign)
                    remote_address[i].port = port_net_order;
                    log_message("INFO:   Remote %d.%d.%d.%d already exists as remote %d (port updated)",
                        ((unsigned char*)&host_ip)[0], ((unsigned char*)&host_ip)[1],
                        ((unsigned char*)&host_ip)[2], ((unsigned char*)&host_ip)[3], i);
                    return TRUE;
                }
            }
        }

        // Find first available slot
        repeat(i, MAX_REMOTE)
        {
            if(remote_on[i] == FALSE)
            {
                log_message("INFO:   Added new remote %d.%d.%d.%d as remote number %d",
                    ((unsigned char*)&host_ip)[0], ((unsigned char*)&host_ip)[1],
                    ((unsigned char*)&host_ip)[2], ((unsigned char*)&host_ip)[3], i);
                remote_address[i].host = host_ip;
                remote_address[i].port = port_net_order;
                remote_on[i] = TRUE;
                remote_ready[i] = FALSE;
                remote_relay[i] = FALSE;
                remote_heartbeat[i] = SDL_GetTicks();
                remote_room_number[i] = 65535;
                remote_is_neighbor[i] = FALSE;
                num_remote++;
                return TRUE;
            }
        }
    }
    return FALSE;
}

//-----------------------------------------------------------------------------------------------
unsigned char network_add_remote_ip(unsigned int host_ip)
{
    // <ZZ> Adds a remote by IP address with default game port (for LAN / backward compat)
    unsigned short default_port;
    #ifdef LIL_ENDIAN
        default_port = (UDP_PORT>>8) | ((UDP_PORT&255)<<8);
    #else
        default_port = UDP_PORT;
    #endif
    return network_add_remote_ip_port(host_ip, default_port);
}

//-----------------------------------------------------------------------------------------------
void network_delete_remote(unsigned short remote)
{
    if(remote < MAX_REMOTE)
    {
        if(remote_on[remote])
        {
            remote_on[remote] = FALSE;
            num_remote--;
        }
    }
}

//-----------------------------------------------------------------------------------------------
void network_cleanup_remote_characters(unsigned int remote_ip)
{
    // Release all characters owned by a remote IP back to local AI control
    unsigned short i;
    repeat(i, MAX_CHARACTER)
    {
        if(main_character_on[i])
        {
            if(*((unsigned int*)(main_character_data[i]+252)) == remote_ip)
            {
                // Clear remote ownership - AI will take over
                *((unsigned int*)(main_character_data[i]+252)) = 0;
                main_character_data[i][250] = 0;
                log_message("INFO:   Released character %d from disconnected remote", i);
            }
        }
    }
}

//-----------------------------------------------------------------------------------------------
void network_disconnect_remote(unsigned short remote)
{
    // Full disconnect: clean up characters, then remove the remote entry
    if(remote < MAX_REMOTE && remote_on[remote])
    {
        unsigned int ip = remote_address[remote].host;
        log_message("INFO:   Disconnecting remote %d (%d.%d.%d.%d)",
            remote,
            ((unsigned char*)&ip)[0], ((unsigned char*)&ip)[1],
            ((unsigned char*)&ip)[2], ((unsigned char*)&ip)[3]);
        network_cleanup_remote_characters(ip);
        network_delete_remote(remote);
    }
}

//-----------------------------------------------------------------------------------------------
void network_peer_tick(void)
{
    // Send peer heartbeats and check for timeouts (time-based, not frame-based)
    unsigned short i;
    unsigned int now = SDL_GetTicks();

    if(!network_on || num_remote == 0) return;

    // Send heartbeat to all remotes periodically
    if(now - peer_heartbeat_last >= PEER_HEARTBEAT_INTERVAL_MS)
    {
        peer_heartbeat_last = now;
        packet_begin(PACKET_TYPE_PEER_HEARTBEAT);
        packet_end();
        network_send(NETWORK_ALL_REMOTES_IN_GAME);
    }

    // Check for timeouts
    repeat(i, MAX_REMOTE)
    {
        if(remote_on[i])
        {
            if(now - remote_heartbeat[i] >= PEER_TIMEOUT_MS)
            {
                log_message("INFO:   Remote %d timed out", i);
                network_disconnect_remote(i);
            }
        }
    }
}

//-----------------------------------------------------------------------------------------------
void network_server_send(void);  // Forward declaration for relay/punch code

void network_punch_update(void)
{
    // <ZZ> Called every frame to send periodic UDP punch packets for NAT traversal
    UDPpacket punch_pkt;
    unsigned int now;

    if(!punch_active) return;

    now = SDL_GetTicks();
    if(now - punch_timer >= PUNCH_INTERVAL_MS)
    {
        punch_timer = now;
        punch_retries++;

        if(punch_retries > PUNCH_MAX_RETRIES)
        {
            // Hole punch failed - request relay from server
            punch_active = FALSE;
            log_message("INFO:   Hole punch failed after %d retries, requesting relay", PUNCH_MAX_RETRIES);
            packet_begin(PACKET_TYPE_RELAY_REQUEST);
            {
                unsigned char* pip = (unsigned char*)&punch_target.host;
                packet_add_unsigned_char(pip[0]);
                packet_add_unsigned_char(pip[1]);
                packet_add_unsigned_char(pip[2]);
                packet_add_unsigned_char(pip[3]);
            }
            packet_end_plain();
            network_server_send();
            // Don't reset join_state - wait for RELAY_DATA confirmation
            return;
        }

        // Send PUNCH_ACK to peer's public endpoint
        packet_begin(PACKET_TYPE_PUNCH_ACK);
            packet_add_unsigned_char(punch_role);
        packet_end_plain();

        punch_pkt.channel = -1;
        punch_pkt.data = packet_buffer;
        punch_pkt.len = packet_length;
        punch_pkt.maxlen = MAX_PACKET_SIZE;
        punch_pkt.address = punch_target;
        SDLNet_UDP_Send(remote_socket, -1, &punch_pkt);
    }
}

//-----------------------------------------------------------------------------------------------
void network_close(void)
{
    // <ZZ> This function shuts down the network...
    log_message("INFO:   Shutting down the network");
    if(lan_broadcast_socket) SDLNet_UDP_Close(lan_broadcast_socket);
    if(remote_socket) SDLNet_UDP_Close(remote_socket);
	SDLNet_Quit();
}

//-----------------------------------------------------------------------------------------------
unsigned char network_setup(void)
{
    // <ZZ> This function initializes all the networking stuff.  Returns TRUE if networking is
    //      available, FALSE if not.
    unsigned short i;


    // Turn all of our ports on and stuff
    network_on = FALSE;


    log_message("INFO:   ------------------------------------------");
    log_message("INFO:   Looking for NETLIST.DAT...");
    netlist = sdf_find_filetype("NETLIST", SDF_FILE_IS_DAT);
    if(netlist)
    {
        netlist = sdf_index_get_data(netlist);
        log_message("INFO:   Found NETLIST.DAT...");
    }



    main_server_on = FALSE;
    network_clear_remote_list();
    log_message("INFO:   ------------------------------------------");
    log_message("INFO:   Trying to turn on networking...");
    if(SDLNet_Init()==0)
    {
        // Network started up okay...
        log_message("INFO:   Network started okay!");


        // Now try to open a UDP socket for talking to other remotes...
        log_message("INFO:   Trying to open up port %d for UDP networking...", UDP_PORT);
        remote_socket=SDLNet_UDP_Open(UDP_PORT);
        if(!remote_socket)
        {
            log_message("ERROR:  Uh, oh...  We couldn't open the port for some reason...  No networking for you...");
        }
        else
        {
            // Resolve master server address if configured (UDP, same socket as game)
            if(MAIN_SERVER_NAME)
            {
                log_message("INFO:   Trying to find the IP address for %s...", MAIN_SERVER_NAME);
                if(SDLNet_ResolveHost(&main_server_address, MAIN_SERVER_NAME, TCPIP_PORT) == 0)
                {
                    log_message("INFO:   Found IP...  It's %d.%d.%d.%d", ((unsigned char*)&main_server_address.host)[0], ((unsigned char*)&main_server_address.host)[1], ((unsigned char*)&main_server_address.host)[2], ((unsigned char*)&main_server_address.host)[3]);
                    main_server_on = TRUE;
                    log_message("INFO:   Master server address resolved.  Will communicate via UDP.");
                }
                else
                {
                    log_message("INFO:   Master server was not found.  LAN and direct IP still available.");
                }
            }
            else
            {
                log_message("INFO:   No master server configured.  LAN and direct IP available.");
            }



            // Find our local LAN IP address
            log_message("INFO:   Looking for IP Address of local machine...");
            local_address.host = LOCALHOST;
#ifdef _WIN32
            {
                ULONG bufsize = 15000;
                PIP_ADAPTER_ADDRESSES addrs = (PIP_ADAPTER_ADDRESSES) malloc(bufsize);
                if(addrs && GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, NULL, addrs, &bufsize) == NO_ERROR)
                {
                    unsigned int fallback_ip = 0;
                    PIP_ADAPTER_ADDRESSES a;
                    for(a = addrs; a != NULL; a = a->Next)
                    {
                        if(a->OperStatus != IfOperStatusUp) continue;
                        if(a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                        PIP_ADAPTER_UNICAST_ADDRESS ua = a->FirstUnicastAddress;
                        if(ua && ua->Address.lpSockaddr->sa_family == AF_INET)
                        {
                            struct sockaddr_in *sa = (struct sockaddr_in *)ua->Address.lpSockaddr;
                            unsigned int ip = sa->sin_addr.s_addr;
                            unsigned char b0 = ((unsigned char*)&ip)[0];
                            unsigned char b1 = ((unsigned char*)&ip)[1];
                            if(b0 == 127) continue;              // loopback
                            if(b0 == 169 && b1 == 254) continue; // APIPA link-local
                            // Prefer adapters with a gateway (real network, not virtual)
                            if(a->FirstGatewayAddress != NULL)
                            {
                                local_address.host = ip;
                                log_message("INFO:   Found LAN IP (with gateway): %d.%d.%d.%d",
                                    ((unsigned char*)&ip)[0], ((unsigned char*)&ip)[1],
                                    ((unsigned char*)&ip)[2], ((unsigned char*)&ip)[3]);
                                break;
                            }
                            else if(fallback_ip == 0)
                            {
                                fallback_ip = ip;
                            }
                        }
                    }
                    if(local_address.host == LOCALHOST && fallback_ip != 0)
                    {
                        local_address.host = fallback_ip;
                        log_message("INFO:   Found LAN IP (no gateway): %d.%d.%d.%d",
                            ((unsigned char*)&fallback_ip)[0], ((unsigned char*)&fallback_ip)[1],
                            ((unsigned char*)&fallback_ip)[2], ((unsigned char*)&fallback_ip)[3]);
                    }
                }
                if(addrs) free(addrs);
            }
#else
            {
                struct ifaddrs *ifaddr, *ifa;
                if(getifaddrs(&ifaddr) == 0)
                {
                    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
                    {
                        if(ifa->ifa_addr == NULL) continue;
                        if(ifa->ifa_addr->sa_family != AF_INET) continue;
                        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                        unsigned int ip = sa->sin_addr.s_addr;
                        // Skip loopback (127.x.x.x)
                        if((((unsigned char*)&ip)[0]) == 127) continue;
                        local_address.host = ip;
                        log_message("INFO:   Found LAN IP on %s: %d.%d.%d.%d",
                            ifa->ifa_name,
                            ((unsigned char*)&ip)[0], ((unsigned char*)&ip)[1],
                            ((unsigned char*)&ip)[2], ((unsigned char*)&ip)[3]);
                        break;
                    }
                    freeifaddrs(ifaddr);
                }
            }
#endif
            if(local_address.host == LOCALHOST)
            {
                log_message("INFO:   Could not find LAN IP, falling back to 127.0.0.1");
            }


            // Open LAN broadcast socket
            lan_broadcast_socket = SDLNet_UDP_Open(LAN_BROADCAST_PORT);
            if(lan_broadcast_socket)
            {
                // Enable SO_BROADCAST on the socket so we can send broadcast packets
                {
                    int sock_fd = 0;
                    // SDL_net stores the socket fd in the UDPsocket struct - extract it
                    // UDPsocket is a pointer to a struct whose first member is the fd
                    struct { int fd; } *sock_internal = (void*)lan_broadcast_socket;
                    sock_fd = sock_internal->fd;
                    if(sock_fd > 0)
                    {
                        int broadcast_enable = 1;
#ifdef _WIN32
                        setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast_enable, sizeof(broadcast_enable));
#else
                        setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
#endif
                        log_message("INFO:   SO_BROADCAST enabled on LAN socket");
                    }
                }
                log_message("INFO:   LAN broadcast socket opened on port %d", LAN_BROADCAST_PORT);
            }
            else
            {
                log_message("INFO:   Could not open LAN broadcast port.  LAN discovery won't work, but direct IP is fine.");
            }

            // Initialize LAN game list
            repeat(i, MAX_LAN_GAMES)
            {
                lan_game_on[i] = FALSE;
                lan_game_timeout[i] = 0;
            }

            network_on = TRUE;
            atexit(network_close);
        }
    }
    else
    {
        // No network this time...
        log_message("ERROR:  Network failed!");
        log_message("ERROR:  SDLNet told us...  %s", SDLNet_GetError());
    }
    log_message("INFO:   ------------------------------------------");
    return network_on;
}

//-----------------------------------------------------------------------------------------------
void network_send(unsigned char send_code)
{
    // <ZZ> This function sends a packet to the specified computer...
    unsigned short i;
    UDPpacket udp_packet;


    // Room tracking not yet implemented - send to all remotes for now
    if(send_code == NETWORK_ALL_REMOTES_IN_ROOM)
    {
        send_code = NETWORK_ALL_REMOTES_IN_GAME;
    }



    if(network_on)
    {
        // Let's figger out who we're sending it to...
        if(send_code == NETWORK_ALL_REMOTES_IN_GAME || send_code == NETWORK_ALL_REMOTES_IN_ROOM || send_code == NETWORK_ALL_REMOTES_IN_NEARBY_ROOMS)
        {
            // Send the packet to all players who need to get it...
            repeat(i, MAX_REMOTE)
            {
                if(remote_on[i])
                {
                    if(send_code == NETWORK_ALL_REMOTES_IN_GAME || remote_room_number[i] == map_current_room || (remote_is_neighbor[i] && send_code == NETWORK_ALL_REMOTES_IN_NEARBY_ROOMS))
                    {
#ifdef ALLOW_LOCAL_PACKETS
                        if(TRUE)
#else
                        if(remote_address[i].host != LOCALHOST && remote_address[i].host != local_address.host)
#endif
                        {
                            if(remote_relay[i])
                            {
                                // Wrap in RELAY_DATA and send through server
                                unsigned short original_len = packet_length;
                                unsigned char relay_temp[MAX_PACKET_SIZE];
                                memcpy(relay_temp, packet_buffer, original_len);

                                log_message("INFO:     Relay send: type=%d len=%d to %d.%d.%d.%d",
                                    relay_temp[0], original_len,
                                    ((unsigned char*)&remote_address[i].host)[0],
                                    ((unsigned char*)&remote_address[i].host)[1],
                                    ((unsigned char*)&remote_address[i].host)[2],
                                    ((unsigned char*)&remote_address[i].host)[3]);

                                packet_begin(PACKET_TYPE_RELAY_DATA);
                                {
                                    unsigned char* pip = (unsigned char*)&remote_address[i].host;
                                    packet_add_unsigned_char(pip[0]);
                                    packet_add_unsigned_char(pip[1]);
                                    packet_add_unsigned_char(pip[2]);
                                    packet_add_unsigned_char(pip[3]);
                                }
                                packet_add_unsigned_short(original_len);
                                memcpy(packet_buffer + packet_length, relay_temp, original_len);
                                packet_length += original_len;
                                packet_end_plain();
                                network_server_send();

                                // Restore original packet for other remotes
                                memcpy(packet_buffer, relay_temp, original_len);
                                packet_length = original_len;
                            }
                            else
                            {
                                udp_packet.channel = -1;
                                udp_packet.data = packet_buffer;
                                udp_packet.len = packet_length;
                                udp_packet.maxlen = MAX_PACKET_SIZE;
                                udp_packet.address.host = remote_address[i].host;
                                udp_packet.address.port = remote_address[i].port;
                                if(!SDLNet_UDP_Send(remote_socket, -1, &udp_packet))
                                {
                                    log_message("INFO:     Got error from SDLNet...  %s", SDLNet_GetError());
                                }
                            }
                        }
                        else
                        {
                            log_message("INFO:     Skipping remote %d because it's the local machine", i);
                        }
                    }
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------------------------
void network_server_send(void)
{
    // <ZZ> Sends the current packet_buffer to the master server via UDP (no encryption)
    UDPpacket udp_packet;

    if(network_on && main_server_on)
    {
        udp_packet.channel = -1;
        udp_packet.data = packet_buffer;
        udp_packet.len = packet_length;
        udp_packet.maxlen = MAX_PACKET_SIZE;
        udp_packet.address.host = main_server_address.host;
        #ifdef LIL_ENDIAN
            udp_packet.address.port = (TCPIP_PORT>>8) | ((TCPIP_PORT&255)<<8);
        #else
            udp_packet.address.port = TCPIP_PORT;
        #endif
        if(!SDLNet_UDP_Send(remote_socket, -1, &udp_packet))
        {
            log_message("INFO:     Server send error: %s", SDLNet_GetError());
        }
    }
}

//-----------------------------------------------------------------------------------------------
unsigned short network_find_remote_character(unsigned int ip_address_of_remote, unsigned char local_index_on_remote)
{
    // <ZZ> This function attempts to find the index of a character on the local computer, who is
    //      hosted on the given remote - with the given local index number on that remote...
    //      This lets me find something like character number 23 on Bob's computer, which is
    //      handled as character 42 on my computer...  It returns the index on my computer, or
    //      MAX_CHARACTER if a match can't be found...
    unsigned short i;
    unsigned char* character_data;

    repeat(i, MAX_CHARACTER)
    {
        if(main_character_on[i])
        {
            character_data = main_character_data[i];
            if(*((unsigned int*)(character_data+252)) == ip_address_of_remote)
            {
                if(character_data[250] == local_index_on_remote)
                {
                    return i;
                }
            }
        }
    }
    return MAX_CHARACTER;
}


//-----------------------------------------------------------------------------------------------
void network_listen(void)
{
    // <ZZ> This function checks for incoming packets, and handles 'em all
    UDPpacket udp_packet;
    unsigned char character_class;
    unsigned short room_number;
    unsigned short seed;
    unsigned char door_flags;
    unsigned short i, num_char;
    signed short length;
    unsigned char* script_file_start;
    float x, y, z;
    unsigned char found;
    unsigned char* character_data;
    unsigned char filename[9];



    if(network_on)
    {
        // Let's try to follow SDLNet's little format...
        udp_packet.channel = -1;
        udp_packet.data = packet_buffer;
        udp_packet.len = MAX_PACKET_SIZE;
        udp_packet.maxlen = MAX_PACKET_SIZE;


        while(SDLNet_UDP_Recv(remote_socket, &udp_packet))
        {
            // We've got a new packet...
            packet_length = udp_packet.len;

            // ServerFu packets (type >= 10) are unencrypted, game packets use encryption
            if(packet_buffer[0] >= PACKET_TYPE_REQUEST_SHARD_LIST)
            {
                // Server packet - validate checksum without decryption
                if(packet_valid())
                {
                    packet_readpos = PACKET_HEADER_SIZE;
                    log_message("INFO:     ServerFu packet type %d", packet_buffer[0]);

                    if(packet_buffer[0] == PACKET_TYPE_REPLY_SHARD_LIST)
                    {
                        // shard_valid_flags(uint), my_ip(uint), then per valid shard: map_server_ip(uint)
                        packet_read_unsigned_int(shard_valid_flags);
                        packet_read_unsigned_int(my_public_ip);
                        for(i = 0; i < MAX_SHARDS; i++)
                        {
                            if(shard_valid_flags & (1u << i))
                            {
                                packet_read_unsigned_int(shard_ip[i]);
                            }
                            else
                            {
                                shard_ip[i] = 0;
                            }
                        }
                        log_message("INFO:     Got shard list (flags=0x%08x)", shard_valid_flags);
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_REPLY_PLAYER_COUNT)
                    {
                        packet_read_unsigned_short(server_player_count);
                        log_message("INFO:     Server player count: %d", server_player_count);
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_REPLY_VERSION_ERROR)
                    {
                        packet_read_unsigned_short(required_executable_version);
                        packet_read_unsigned_short(required_data_version);
                        global_version_error = TRUE;
                        log_message("ERROR:  Version mismatch! Server requires exe=%d data=%d",
                            required_executable_version, required_data_version);
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_COMMAND_JOIN)
                    {
                        // Server is telling us about a join
                        unsigned char continent, direction, letter, pw_ok;
                        unsigned int joiner_ip;
                        packet_read_unsigned_char(continent);
                        packet_read_unsigned_char(direction);
                        packet_read_unsigned_char(letter);
                        packet_read_unsigned_char(pw_ok);
                        packet_read_unsigned_int(joiner_ip);

                        // Check if this COMMAND_JOIN is about ourselves
                        // When the server sends our own COMMAND_JOIN back, joiner_ip is our PUBLIC IP
                        // which may differ from local_address.host (LAN IP)
                        // After hole punch: joiner has join_state==2, num_remote>0, and receives
                        // COMMAND_JOIN from the host directly (not from server)
                        if(joiner_ip == local_address.host || joiner_ip == my_public_ip ||
                           (udp_packet.address.host == main_server_address.host && join_state >= 1 && !lan_hosting && num_remote == 0) ||
                           (join_state == 2 && !lan_hosting && num_remote > 0))
                        {
                            // Learn our public IP from the server
                            if(my_public_ip == 0 && joiner_ip != local_address.host &&
                               udp_packet.address.host == main_server_address.host)
                            {
                                my_public_ip = joiner_ip;
                                log_message("INFO:     Learned our public IP: %d.%d.%d.%d",
                                    ((unsigned char*)&my_public_ip)[0], ((unsigned char*)&my_public_ip)[1],
                                    ((unsigned char*)&my_public_ip)[2], ((unsigned char*)&my_public_ip)[3]);
                            }
                            // Only process if we're actually trying to join (not hosting)
                            if(!lan_hosting && join_state >= 1 && pw_ok == PASSWORD_OKAY_VALUE)
                            {
                                // Read game seed if present
                                if(packet_readpos + 4 <= packet_length)
                                {
                                    packet_read_unsigned_int(game_seed);
                                    log_message("INFO:     Got game seed: %u", game_seed);
                                }
                                // If this came from a peer directly (hole punch), go straight to connected
                                if(udp_packet.address.host != main_server_address.host && num_remote > 0)
                                {
                                    main_game_active = TRUE;
                                    join_state = 4;
                                    log_message("INFO:     Join accepted via hole punch (direct COMMAND_JOIN)");
                                }
                                else
                                {
                                    // Send REPLY_ROGER back to server
                                    packet_begin(PACKET_TYPE_REPLY_ROGER);
                                    packet_end_plain();
                                    network_server_send();
                                    join_state = 2;
                                    log_message("INFO:     Join accepted, sent roger");
                                }
                            }
                        }
                        // If the packet came from the master server and we're hosting,
                        // the joiner might be us (our public IP that we don't know yet)
                        else if(udp_packet.address.host == main_server_address.host && my_public_ip == 0)
                        {
                            // Learn our public IP and ignore
                            my_public_ip = joiner_ip;
                            log_message("INFO:     Learned our public IP from server: %d.%d.%d.%d",
                                ((unsigned char*)&my_public_ip)[0], ((unsigned char*)&my_public_ip)[1],
                                ((unsigned char*)&my_public_ip)[2], ((unsigned char*)&my_public_ip)[3]);
                        }
                        else if(lan_hosting || num_remote > 0)
                        {
                            // We're in the game - add this joiner as a remote
                            network_add_remote_ip(joiner_ip);
                            log_message("INFO:     Added joiner %d.%d.%d.%d to game",
                                ((unsigned char*)&joiner_ip)[0], ((unsigned char*)&joiner_ip)[1],
                                ((unsigned char*)&joiner_ip)[2], ((unsigned char*)&joiner_ip)[3]);

                            // Send COMMAND_JOIN to the joiner directly (peer-to-peer)
                            packet_begin(PACKET_TYPE_COMMAND_JOIN);
                                packet_add_unsigned_char(continent);
                                packet_add_unsigned_char(direction);
                                packet_add_unsigned_char(letter);
                                packet_add_unsigned_char(pw_ok);
                                packet_add_unsigned_int(joiner_ip);
                                packet_add_unsigned_int(game_seed);
                            packet_end_plain();
                            {
                                UDPpacket reply_packet;
                                reply_packet.channel = -1;
                                reply_packet.data = packet_buffer;
                                reply_packet.len = packet_length;
                                reply_packet.maxlen = MAX_PACKET_SIZE;
                                reply_packet.address.host = joiner_ip;
                                #ifdef LIL_ENDIAN
                                    reply_packet.address.port = (UDP_PORT>>8) | ((UDP_PORT&255)<<8);
                                #else
                                    reply_packet.address.port = UDP_PORT;
                                #endif
                                SDLNet_UDP_Send(remote_socket, -1, &reply_packet);
                            }

                            // Notify main server that we handled it
                            packet_begin(PACKET_TYPE_COMMAND_JOIN);
                                packet_add_unsigned_char(continent);
                                packet_add_unsigned_char(direction);
                                packet_add_unsigned_char(letter);
                                packet_add_unsigned_char(pw_ok);
                                packet_add_unsigned_int(joiner_ip);
                            packet_end_plain();
                            network_server_send();
                        }
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_REPLY_JOIN_OKAY)
                    {
                        unsigned int sun_time;
                        packet_read_unsigned_int(sun_time);
                        if(join_state >= 1)
                        {
                            main_game_active = TRUE;
                            if(num_remote == 0 && !lan_hosting)
                            {
                                // First player on server — become the host
                                lan_hosting = TRUE;
                                lan_broadcast_timer = 0;
                                join_state = 5;  // Hosting via server
                                log_message("INFO:     Join complete — we are the host! (sun_time=%u)", sun_time);
                            }
                            else
                            {
                                join_state = 4;  // Connected as joiner
                                log_message("INFO:     Join complete! (sun_time=%u)", sun_time);
                            }

                            // Request IP lists to learn about other machines
                            for(i = 0; i < 16; i++)
                            {
                                packet_begin(PACKET_TYPE_REQUEST_IP_LIST);
                                    packet_add_unsigned_char((unsigned char) i);
                                packet_end_plain();
                                network_server_send();
                            }
                        }
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_REPLY_IP_LIST)
                    {
                        unsigned char portion;
                        unsigned char rx, ry, rz, pw;
                        unsigned int machine_ip;
                        packet_read_unsigned_char(portion);
                        log_message("INFO:     Got IP list portion %d", portion);

                        // Read machine entries (up to 64 per portion)
                        while(packet_readpos + 8 <= packet_length)
                        {
                            packet_read_unsigned_char(rx);
                            packet_read_unsigned_char(ry);
                            packet_read_unsigned_char(rz);
                            packet_read_unsigned_char(pw);
                            packet_read_unsigned_int(machine_ip);
                            if(machine_ip != 0 && machine_ip != local_address.host && machine_ip != my_public_ip)
                            {
                                network_add_remote_ip(machine_ip);
                            }
                        }
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_REPORT_MACHINE_DOWN)
                    {
                        unsigned char continent, direction, letter;
                        unsigned int down_ip;
                        packet_read_unsigned_char(continent);
                        packet_read_unsigned_char(direction);
                        packet_read_unsigned_char(letter);
                        packet_read_unsigned_int(down_ip);
                        log_message("INFO:     Machine down: %d.%d.%d.%d",
                            ((unsigned char*)&down_ip)[0], ((unsigned char*)&down_ip)[1],
                            ((unsigned char*)&down_ip)[2], ((unsigned char*)&down_ip)[3]);

                        // Find and remove this remote
                        repeat(i, MAX_REMOTE)
                        {
                            if(remote_on[i] && remote_address[i].host == down_ip)
                            {
                                network_delete_remote(i);
                                break;
                            }
                        }
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_RELAY_DATA)
                    {
                        // Relay data from the master server
                        unsigned int relay_peer_ip;
                        unsigned short relay_blob_len;
                        // Read IP as raw bytes to preserve network byte order
                        {
                            unsigned char ip_bytes[4];
                            packet_read_unsigned_char(ip_bytes[0]);
                            packet_read_unsigned_char(ip_bytes[1]);
                            packet_read_unsigned_char(ip_bytes[2]);
                            packet_read_unsigned_char(ip_bytes[3]);
                            memcpy(&relay_peer_ip, ip_bytes, 4);
                        }
                        packet_read_unsigned_short(relay_blob_len);

                        if(relay_blob_len == 0)
                        {
                            // Relay established confirmation
                            log_message("INFO:     Relay established with peer %d.%d.%d.%d",
                                ((unsigned char*)&relay_peer_ip)[0], ((unsigned char*)&relay_peer_ip)[1],
                                ((unsigned char*)&relay_peer_ip)[2], ((unsigned char*)&relay_peer_ip)[3]);

                            // Add peer as remote and mark as relayed
                            network_add_remote_ip_port(relay_peer_ip, 0);
                            repeat(i, MAX_REMOTE)
                            {
                                if(remote_on[i] && remote_address[i].host == relay_peer_ip)
                                {
                                    remote_relay[i] = TRUE;
                                    break;
                                }
                            }

                            // Continue join flow like punch succeeded
                            if(punch_role == 1)
                            {
                                // We are the host
                                lan_hosting = TRUE;
                                main_game_active = TRUE;
                                lan_broadcast_timer = 0;

                                packet_begin(PACKET_TYPE_COMMAND_JOIN);
                                    packet_add_unsigned_char(punch_continent);
                                    packet_add_unsigned_char(punch_direction);
                                    packet_add_unsigned_char(punch_letter);
                                    packet_add_unsigned_char(punch_pw_ok);
                                    {
                                        unsigned int self_ip = local_address.host;
                                        packet_add_unsigned_int(self_ip);
                                    }
                                    packet_add_unsigned_int(game_seed);
                                packet_end_plain();
                                // Send via relay
                                {
                                    unsigned short cmd_len = packet_length;
                                    unsigned char cmd_buf[MAX_PACKET_SIZE];
                                    memcpy(cmd_buf, packet_buffer, cmd_len);
                                    packet_begin(PACKET_TYPE_RELAY_DATA);
                                    {
                                        unsigned char* pip = (unsigned char*)&relay_peer_ip;
                                        packet_add_unsigned_char(pip[0]);
                                        packet_add_unsigned_char(pip[1]);
                                        packet_add_unsigned_char(pip[2]);
                                        packet_add_unsigned_char(pip[3]);
                                    }
                                    packet_add_unsigned_short(cmd_len);
                                    memcpy(packet_buffer + packet_length, cmd_buf, cmd_len);
                                    packet_length += cmd_len;
                                    packet_end_plain();
                                    network_server_send();
                                }

                                // Notify master server
                                packet_begin(PACKET_TYPE_COMMAND_JOIN);
                                    packet_add_unsigned_char(punch_continent);
                                    packet_add_unsigned_char(punch_direction);
                                    packet_add_unsigned_char(punch_letter);
                                    packet_add_unsigned_char(punch_pw_ok);
                                    {
                                        unsigned char* pip = (unsigned char*)&relay_peer_ip;
                                        packet_add_unsigned_char(pip[0]);
                                        packet_add_unsigned_char(pip[1]);
                                        packet_add_unsigned_char(pip[2]);
                                        packet_add_unsigned_char(pip[3]);
                                    }
                                packet_end_plain();
                                network_server_send();

                                join_state = 5;
                                log_message("INFO:     We are the host via relay");
                            }
                            else
                            {
                                join_state = 2;
                                log_message("INFO:     We are the joiner via relay, waiting for COMMAND_JOIN");
                            }
                        }
                        else
                        {
                            // Unwrap relayed game data and process as if it arrived from peer
                            log_message("INFO:     Relay recv: blob_len=%d from %d.%d.%d.%d readpos=%d pktlen=%d",
                                relay_blob_len,
                                ((unsigned char*)&relay_peer_ip)[0], ((unsigned char*)&relay_peer_ip)[1],
                                ((unsigned char*)&relay_peer_ip)[2], ((unsigned char*)&relay_peer_ip)[3],
                                packet_readpos, packet_length);
                            if(packet_readpos + relay_blob_len <= packet_length)
                            {
                                // Reset heartbeat for this relayed remote
                                unsigned char heartbeat_found = FALSE;
                                repeat(i, MAX_REMOTE)
                                {
                                    if(remote_on[i] && remote_address[i].host == relay_peer_ip)
                                    {
                                        remote_heartbeat[i] = SDL_GetTicks();
                                        heartbeat_found = TRUE;
                                        break;
                                    }
                                }
                                log_message("INFO:     Relay heartbeat reset: %s", heartbeat_found ? "yes" : "no - remote not found");

                                memmove(packet_buffer, packet_buffer + packet_readpos, relay_blob_len);
                                packet_length = relay_blob_len;
                                udp_packet.address.host = relay_peer_ip;

                                // Process as server or game packet
                                if(packet_buffer[0] >= PACKET_TYPE_REQUEST_SHARD_LIST)
                                {
                                    // ServerFu packet (e.g. COMMAND_JOIN sent via relay)
                                    if(packet_valid())
                                    {
                                        packet_readpos = PACKET_HEADER_SIZE;
                                        // Re-enter server packet handling for this unwrapped packet
                                        // Handle COMMAND_JOIN specifically for relay join flow
                                        if(packet_buffer[0] == PACKET_TYPE_COMMAND_JOIN)
                                        {
                                            unsigned char r_continent, r_direction, r_letter, r_pw_ok;
                                            unsigned int r_joiner_ip;
                                            packet_read_unsigned_char(r_continent);
                                            packet_read_unsigned_char(r_direction);
                                            packet_read_unsigned_char(r_letter);
                                            packet_read_unsigned_char(r_pw_ok);
                                            packet_read_unsigned_int(r_joiner_ip);
                                            if(join_state >= 1 && r_pw_ok == PASSWORD_OKAY_VALUE)
                                            {
                                                if(packet_readpos + 4 <= packet_length)
                                                {
                                                    packet_read_unsigned_int(game_seed);
                                                    log_message("INFO:     Got game seed via relay: %u", game_seed);
                                                }
                                                packet_begin(PACKET_TYPE_REPLY_ROGER);
                                                packet_end_plain();
                                                network_server_send();
                                                join_state = 4;
                                                main_game_active = TRUE;
                                                log_message("INFO:     Join accepted via relay");
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    // Encrypted game packet - decrypt and process
                                    packet_decrypt();
                                    if(packet_valid())
                                    {
                                        packet_readpos = PACKET_HEADER_SIZE;
                                        // Jump into game packet processing
                                        goto relay_process_game_packet;
                                    }
                                }
                            }
                        }
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_PUNCH_ACK)
                    {
                        // Peer responded to our punch - NAT hole is open!
                        if(punch_active)
                        {
                            punch_active = FALSE;
                            log_message("INFO:     Hole punch succeeded! Peer reached us.");

                            // Add peer as remote using their actual source address
                            network_add_remote_ip_port(udp_packet.address.host, udp_packet.address.port);

                            if(punch_role == 1)
                            {
                                // We are the host - send COMMAND_JOIN to the joiner through the punched hole
                                lan_hosting = TRUE;
                                main_game_active = TRUE;
                                lan_broadcast_timer = 0;

                                packet_begin(PACKET_TYPE_COMMAND_JOIN);
                                    packet_add_unsigned_char(punch_continent);
                                    packet_add_unsigned_char(punch_direction);
                                    packet_add_unsigned_char(punch_letter);
                                    packet_add_unsigned_char(punch_pw_ok);
                                    // Send our own IP so joiner knows who we are
                                    {
                                        unsigned int self_ip = local_address.host;
                                        packet_add_unsigned_int(self_ip);
                                    }
                                    packet_add_unsigned_int(game_seed);
                                packet_end_plain();
                                {
                                    UDPpacket reply_pkt;
                                    reply_pkt.channel = -1;
                                    reply_pkt.data = packet_buffer;
                                    reply_pkt.len = packet_length;
                                    reply_pkt.maxlen = MAX_PACKET_SIZE;
                                    reply_pkt.address = udp_packet.address;
                                    SDLNet_UDP_Send(remote_socket, -1, &reply_pkt);
                                }

                                // Notify master server
                                packet_begin(PACKET_TYPE_COMMAND_JOIN);
                                    packet_add_unsigned_char(punch_continent);
                                    packet_add_unsigned_char(punch_direction);
                                    packet_add_unsigned_char(punch_letter);
                                    packet_add_unsigned_char(punch_pw_ok);
                                    {
                                        unsigned int peer_ip = udp_packet.address.host;
                                        packet_add_unsigned_int(peer_ip);
                                    }
                                packet_end_plain();
                                network_server_send();

                                join_state = 5;  // Hosting
                                log_message("INFO:     We are the host after hole punch");
                            }
                            else
                            {
                                // We are the joiner - host will send us COMMAND_JOIN shortly
                                join_state = 2;
                                log_message("INFO:     We are the joiner after hole punch, waiting for COMMAND_JOIN");
                            }
                        }
                    }
                    else if(packet_buffer[0] == PACKET_TYPE_PUNCH_REQUEST)
                    {
                        // Master server is telling us to punch through NAT to a peer
                        unsigned int peer_ip;
                        unsigned short peer_port_host;
                        // Read IP as raw bytes to preserve network byte order
                        // (server sends via memcpy, not big-endian encoded)
                        {
                            unsigned char ip_bytes[4];
                            packet_read_unsigned_char(ip_bytes[0]);
                            packet_read_unsigned_char(ip_bytes[1]);
                            packet_read_unsigned_char(ip_bytes[2]);
                            packet_read_unsigned_char(ip_bytes[3]);
                            memcpy(&peer_ip, ip_bytes, 4);
                        }
                        packet_read_unsigned_short(peer_port_host);
                        packet_read_unsigned_char(punch_continent);
                        packet_read_unsigned_char(punch_direction);
                        packet_read_unsigned_char(punch_letter);
                        packet_read_unsigned_char(punch_pw_ok);
                        packet_read_unsigned_char(punch_role);

                        punch_target.host = peer_ip;
                        #ifdef LIL_ENDIAN
                            punch_target.port = (peer_port_host>>8) | ((peer_port_host&255)<<8);
                        #else
                            punch_target.port = peer_port_host;
                        #endif

                        punch_active = TRUE;
                        punch_timer = SDL_GetTicks();
                        punch_retries = 0;

                        log_message("INFO:     Got PUNCH_REQUEST: peer=%d.%d.%d.%d:%d role=%d",
                            ((unsigned char*)&peer_ip)[0], ((unsigned char*)&peer_ip)[1],
                            ((unsigned char*)&peer_ip)[2], ((unsigned char*)&peer_ip)[3],
                            peer_port_host, punch_role);

                        // Send first punch immediately
                        {
                            UDPpacket punch_pkt;
                            packet_begin(PACKET_TYPE_PUNCH_ACK);
                                packet_add_unsigned_char(punch_role);
                            packet_end_plain();
                            punch_pkt.channel = -1;
                            punch_pkt.data = packet_buffer;
                            punch_pkt.len = packet_length;
                            punch_pkt.maxlen = MAX_PACKET_SIZE;
                            punch_pkt.address = punch_target;
                            SDLNet_UDP_Send(remote_socket, -1, &punch_pkt);
                        }
                    }
                }
            }
            else
            {
            // Game packet - decrypt and validate
            packet_decrypt();
            if(packet_valid())
            {
                // Reset heartbeat timer for this sender
                {
                    unsigned short ri;
                    repeat(ri, MAX_REMOTE)
                    {
                        if(remote_on[ri] && remote_address[ri].host == udp_packet.address.host)
                        {
                            remote_heartbeat[ri] = SDL_GetTicks();
                            break;
                        }
                    }
                }
                // Screen out any packets we accidentally sent to ourself...
#ifdef ALLOW_LOCAL_PACKETS
                if(TRUE)
#else
                if(udp_packet.address.host != LOCALHOST && udp_packet.address.host != local_address.host)
#endif
                {
                    relay_process_game_packet:
                    packet_readpos = PACKET_HEADER_SIZE;
                    if(packet_buffer[0] == PACKET_TYPE_CHAT)
                    {
                        packet_read_unsigned_char(character_class);     // Speaker class
                        packet_read_string(run_string[0]);              // Speaker name
                        packet_read_string(run_string[1]);              // Message
                        message_add(run_string[1], run_string[0], TRUE);
                    }
                    if(packet_buffer[0] == PACKET_TYPE_ROOM_UPDATE)
                    {
                        packet_read_unsigned_short(room_number);        // The room number this sender is in...
                        packet_read_unsigned_short(seed);               // The map seed this sender is using...
                        // If remote player is in a different room, silently despawn their characters
                        if(room_number != map_current_room)
                        {
                            repeat(i, MAX_CHARACTER)
                            {
                                if(main_character_on[i])
                                {
                                    character_data = main_character_data[i];
                                    if(*((unsigned int*)(character_data+252)) == udp_packet.address.host)
                                    {
                                        main_character_on[i] = FALSE;
                                    }
                                }
                            }
                        }
                        // TODO: Seed should be checked properly
                        if(room_number == map_current_room && seed == 0 && netlist)
                        {
                            // Start to kill off any of this host's characters...
                            repeat(i, MAX_CHARACTER)
                            {
                                if(main_character_on[i])
                                {
                                    character_data = main_character_data[i];
                                    if(*((unsigned int*)(character_data+252)) == udp_packet.address.host)
                                    {
                                        character_data[82] = 0;  // Give 'em 0 hits...
                                    }
                                }
                            }



                            packet_read_unsigned_char(door_flags);      // The door flags for this room...
                            packet_read_unsigned_char(num_char);        // The number of characters in this packet...  (each character should have 11 or 19 bytes of data...)
                            length = packet_length - packet_readpos;    // The number of bytes remaining...
                            while(num_char > 0 && length >= 11)
                            {
                                network_script_newly_spawned = FALSE;
                                packet_read_unsigned_char(network_script_remote_index);
                                packet_read_unsigned_char(network_script_netlist_index);
                                packet_read_unsigned_char(network_script_z);
                                packet_read_unsigned_char(network_script_x);
                                packet_read_unsigned_char(network_script_y);
                                network_script_x = network_script_x | ((network_script_z&192)<<2);
                                network_script_y = network_script_y | ((network_script_z&48)<<4);
                                network_script_z = network_script_z & 15;
                                packet_read_unsigned_char(network_script_facing);
                                packet_read_unsigned_char(network_script_action);
                                network_script_extra_data = network_script_action>>7;
                                network_script_action = network_script_action&127;
                                packet_read_unsigned_char(network_script_team);
                                network_script_poison = (network_script_team >> 5) & 1;
                                network_script_petrify = (network_script_team >> 4) & 1;
                                network_script_alpha = (network_script_team & 8) ? (64) : (255);
                                network_script_deflect = (network_script_team >> 2) & 1;
                                network_script_haste = (network_script_team >> 1) & 1;
                                network_script_other_enchant = (network_script_team & 1);
                                network_script_team = network_script_team >> 6;
                                packet_read_unsigned_char(network_script_eqleft);
                                packet_read_unsigned_char(network_script_eqright);
                                packet_read_unsigned_char(network_script_eqcol01);
                                if(network_script_extra_data && length >= 19)
                                {
                                    // We've got more data coming...
                                    packet_read_unsigned_char(network_script_eqcol23);
                                    packet_read_unsigned_char(network_script_eqspec1);
                                    packet_read_unsigned_char(network_script_eqspec2);
                                    packet_read_unsigned_char(network_script_eqhelm);
                                    packet_read_unsigned_char(network_script_eqbody);
                                    packet_read_unsigned_char(network_script_eqlegs);
                                    packet_read_unsigned_char(network_script_class);
                                    packet_read_unsigned_char(network_script_mount_index);
                                    if(network_script_mount_index != network_script_remote_index)
                                    {
                                        // Character is riding a mount...
                                        network_script_mount_index = network_find_remote_character(udp_packet.address.host, (unsigned char) network_script_mount_index);
                                    }
                                    else
                                    {
                                        network_script_mount_index = MAX_CHARACTER;
                                    }
                                }
                                else
                                {
                                    // Script shouldn't ask for these, but just in case...
                                    network_script_eqcol23 = 0;
                                    network_script_eqspec1 = 0;
                                    network_script_eqspec2 = 0;
                                    network_script_eqhelm = 0;
                                    network_script_eqbody = 0;
                                    network_script_eqlegs = 0;
                                    network_script_class = 0;
                                    network_script_mount_index = MAX_CHARACTER;
                                }


                                // Okay, we've read all of the data for this character, now let's see if we need to spawn it...
                                i = network_find_remote_character(udp_packet.address.host, network_script_remote_index);
                                found = FALSE;
                                if(i < MAX_CHARACTER)
                                {
                                    if(main_character_on[i])
                                    {
                                        // Looks like we've found the character...
                                        found = TRUE;
                                    }
                                }
                                if(!found)
                                {
                                    // We didn't find this character - that means we'll have to try to spawn a new one of the appropriate type...
                                    x = (network_script_x - 512.0f) * 0.25f;
                                    y = (network_script_y - 512.0f) * 0.25f;
                                    z = room_heightmap_height(roombuffer, x, y);
                                    z = z + (network_script_z*2.0f);


                                    script_file_start = netlist + (network_script_netlist_index<<3);
                                    repeat(i, 8)
                                    {
                                        filename[i] = script_file_start[i];
                                    }
                                    filename[8] = 0;
                                    i = MAX_CHARACTER;
                                    script_file_start = sdf_find_filetype(filename, SDF_FILE_IS_RUN);
                                    if(script_file_start)
                                    {
                                        script_file_start = sdf_index_get_data(script_file_start);
                                        character_data = obj_spawn(CHARACTER, x, y, z, script_file_start, 65535);
                                        if(character_data)
                                        {
                                            if(character_data >= main_character_data[0] && character_data <= main_character_data[MAX_CHARACTER-1])
                                            {
                                                network_script_newly_spawned = TRUE;
                                                i = (character_data-main_character_data[0])/CHARACTER_SIZE;
                                                i = i & (MAX_CHARACTER-1);
                                                *((unsigned int*)(character_data+252)) = udp_packet.address.host;
                                                character_data[250] = network_script_remote_index;
                                            }
                                        }
                                    }
                                }
                                // Now let's do this again and give the character a script function call, so we can handle the network data more precisely...
                                if(i < MAX_CHARACTER)
                                {
                                    if(main_character_on[i])
                                    {
                                        character_data = main_character_data[i];
                                        character_data[67] = EVENT_NETWORK_UPDATE;
                                        fast_run_script(main_character_script_start[i], FAST_FUNCTION_EVENT, character_data);
                                    }
                                }
                                





                                num_char--;
                                length = packet_length - packet_readpos;
                            }




                            // Despawn any character from this remote whose hits are still 0
                            // (meaning they weren't included in the update - they left or died)
                            repeat(i, MAX_CHARACTER)
                            {
                                if(main_character_on[i])
                                {
                                    character_data = main_character_data[i];
                                    if(*((unsigned int*)(character_data+252)) == udp_packet.address.host)
                                    {
                                        if(character_data[82] == 0)
                                        {
                                            main_character_on[i] = FALSE;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if(packet_buffer[0] == PACKET_TYPE_I_WANNA_PLAY)
                    {
                        // Someone wants to join our game
                        unsigned int remote_seed;
                        packet_read_unsigned_int(remote_seed);
                        log_message("INFO:   Got I_WANNA_PLAY from %d.%d.%d.%d (seed %d)",
                            ((unsigned char*)&udp_packet.address.host)[0],
                            ((unsigned char*)&udp_packet.address.host)[1],
                            ((unsigned char*)&udp_packet.address.host)[2],
                            ((unsigned char*)&udp_packet.address.host)[3],
                            remote_seed);

                        if(lan_hosting)
                        {
                            // Accept the player - clean up any stale characters from a previous connection
                            UDPpacket reply_packet;
                            network_cleanup_remote_characters(udp_packet.address.host);
                            network_add_remote_ip(udp_packet.address.host);

                            // Send OKAY_YOU_CAN_PLAY reply with our game seed
                            packet_begin(PACKET_TYPE_OKAY_YOU_CAN_PLAY);
                                packet_add_unsigned_int(game_seed);
                            packet_end();

                            reply_packet.channel = -1;
                            reply_packet.data = packet_buffer;
                            reply_packet.len = packet_length;
                            reply_packet.maxlen = MAX_PACKET_SIZE;
                            reply_packet.address.host = udp_packet.address.host;
                            #ifdef LIL_ENDIAN
                                reply_packet.address.port = (UDP_PORT>>8) | ((UDP_PORT&255)<<8);
                            #else
                                reply_packet.address.port = UDP_PORT;
                            #endif
                            SDLNet_UDP_Send(remote_socket, -1, &reply_packet);
                            log_message("INFO:   Sent OKAY_YOU_CAN_PLAY reply");
                        }
                    }
                    if(packet_buffer[0] == PACKET_TYPE_OKAY_YOU_CAN_PLAY)
                    {
                        // Host accepted our join request
                        unsigned int host_seed;
                        packet_read_unsigned_int(host_seed);
                        log_message("INFO:   Got OKAY_YOU_CAN_PLAY (host seed %d)", host_seed);

                        if(join_state >= 1)
                        {
                            // Use the host's game seed - clean up stale data from any previous connection
                            game_seed = host_seed;
                            network_cleanup_remote_characters(udp_packet.address.host);
                            network_add_remote_ip(udp_packet.address.host);
                            join_state = 4;  // Connected, waiting in lobby
                            main_game_active = TRUE;
                            log_message("INFO:   Successfully joined game!");
                        }
                    }
                    if(packet_buffer[0] == PACKET_TYPE_PLAYER_READY)
                    {
                        // A remote player signaled they're ready
                        repeat(i, MAX_REMOTE)
                        {
                            if(remote_on[i] && remote_address[i].host == udp_packet.address.host)
                            {
                                remote_ready[i] = TRUE;
                                log_message("INFO:   Remote %d is ready", i);
                                break;
                            }
                        }
                    }
                    if(packet_buffer[0] == PACKET_TYPE_START_GAME)
                    {
                        // Host says game is starting
                        if(join_state == 4 || join_state == 5)
                        {
                            join_state = 6;  // Game started by host
                            log_message("INFO:   Host started the game!");
                        }
                    }
                }
            }
            } // end else (game packet)
        }
    }
}

//-----------------------------------------------------------------------------------------------
unsigned char network_find_script_index(unsigned char* filename)
{
    // <ZZ> This function finds a character script filename in the NETLIST.DAT file...  So
    //      we don't have to send the whole thing over the network...  Returns 0 if it didn't
    //      find a match...
    unsigned short i, j;
    unsigned char found;
    unsigned char* checkname;
    if(netlist)
    {
        checkname = netlist+8;
        i = 1;
        while(i < 256)
        {
            found = TRUE;
            repeat(j, 8)
            {
                if(checkname[j] == filename[j])
                {
                    if(checkname[j] == 0)
                    {
                        return ((unsigned char) i);
                    }
                }
                else
                {
                    found = FALSE;
                    j = 8;
                }
            }
            if(found)
            {
                return ((unsigned char) i);
            }
            checkname+=8;
            i++;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------------------------












//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
// Special functions to send certain types of packets...
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void network_send_chat(unsigned char speaker_class, unsigned char* speaker_name, unsigned char* message)
{
    // <ZZ> This function sends a chat message to all the players in the room (or if message starts with
    //      <ALL> it goes to all in the game)...
    unsigned char send_to_all;

    send_to_all = FALSE;
    if(message)
    {
        if(message[0] == '<')
        {
            if(message[1] == 'A')
            {
                if(message[2] == 'L')
                {
                    if(message[3] == 'L')
                    {
                        if(message[4] == '>')
                        {
                            send_to_all = TRUE;
                            message+=5;
                            if(message[0] == ' ')
                            {
                                message++;
                            }
                        }
                    }
                }
            }
        }
    }
    packet_begin(PACKET_TYPE_CHAT);
        packet_add_unsigned_char(speaker_class);
        packet_add_string(speaker_name);
        packet_add_string(message);
    packet_end();


    // Spit out the message on the local computer...
    message_add(message, speaker_name, TRUE);


    if(send_to_all)
    {
        log_message("INFO:   Sending chat message %s to all in game from speaker %s", message, speaker_name);
        network_send(NETWORK_ALL_REMOTES_IN_GAME);
    }
    else
    {
        log_message("INFO:   Sending chat message %s to all in room from speaker %s", message, speaker_name);
        network_send(NETWORK_ALL_REMOTES_IN_ROOM);
    }
}

//-----------------------------------------------------------------------------------------------
void network_send_room_update()
{
    // <ZZ> This function sends a room update packet to all the players in the room...
    //      The packet should include all characters that are hosted on the local machine...
    unsigned short local_character_count, i, facing;
    unsigned char* character_data;
    unsigned short mount;
    float* character_xyz;
    unsigned short x, y;
    float fz;
    unsigned char z;
    unsigned char pmod;
    unsigned char action;
    unsigned char misc;

    // Make sure we're in a valid room...
    if(map_current_room < MAX_MAP_ROOM)
    {
        // Count how many characters we need to send over network...
        local_character_count = 0;
        repeat(i, MAX_CHARACTER)
        {
            // Only need to send characters that are used...
            if(main_character_on[i])
            {
                // Only need to send characters that are hosted locally...
                if(main_character_data[i][252] == 0 && main_character_data[i][253] == 0 && main_character_data[i][254] == 0 && main_character_data[i][255] == 0)
                {
                    // Only send player characters (CHAR_FULL_NETWORK), not room entities
                    // Room entities (monsters, crates, etc.) are generated identically on all machines from shared seed
                    if(main_character_data[i][251] && (*((unsigned short*)(main_character_data[i]+60)) & 4096))
                    {
                        // Looks like we've got one to send...
                        local_character_count++;
                    }
                }
            }
        }


        if(local_character_count > 255)
        {
            local_character_count = 255;
        }
        // Only log when character count changes to avoid spam
        {
            static unsigned short last_count = 65535;
            if(local_character_count != last_count)
            {
                log_message("INFO:   Sending room update packet to all in room (%d characters)", local_character_count);
                last_count = local_character_count;
            }
        }


        packet_begin(PACKET_TYPE_ROOM_UPDATE);
            packet_add_unsigned_short(map_current_room);
            packet_add_unsigned_short(0);       // !!!BAD!!!  map_seed
            packet_add_unsigned_char(map_room_data[map_current_room][29]);  // Door flags
            packet_add_unsigned_char(local_character_count);
            i = 0;
            while(i < MAX_CHARACTER && local_character_count > 0)
            {
                if(main_character_on[i])
                {
                    if(main_character_data[i][252] == 0 && main_character_data[i][253] == 0 && main_character_data[i][254] == 0 && main_character_data[i][255] == 0)
                    {
                        if(main_character_data[i][251] && (*((unsigned short*)(main_character_data[i]+60)) & 4096))
                        {
                            // This character is hosted on the local machine, so let's send it on over...
                            character_data = main_character_data[i];
                            character_xyz = (float*) character_data;
                            packet_add_unsigned_char(i);                        // Local index number
                            packet_add_unsigned_char(character_data[251]);      // Script index (in NETLIST.DAT)
                            x = ((unsigned short) ((character_xyz[X]*ROOM_HEIGHTMAP_PRECISION) + 512.0f))&1023;
                            y = ((unsigned short) ((character_xyz[Y]*ROOM_HEIGHTMAP_PRECISION) + 512.0f))&1023;
                            fz = (character_xyz[Z] - character_xyz[11])*0.5f;  clip(0.0f, fz, 15.0f);  z = (unsigned char) fz;
                            pmod = ((x>>8)<<6) | ((y>>8)<<4) | z;
                            packet_add_unsigned_char(pmod);                     // Position modifiers (top 2 bits for x) (mid 2 bits for y) (low 4 bits are z above floor)
                            packet_add_unsigned_char(x);                        // X position (with modifier should range from 0-1023)
                            packet_add_unsigned_char(y);                        // Y position (with modifier should range from 0-1023)
                            facing = *((unsigned short*) (character_data+56));
                            facing = facing>>8;
                            packet_add_unsigned_char(facing);                   // Facing (should range from 0-255)
                            action = character_data[65];
                            if(CHAR_FLAGS & CHAR_FULL_NETWORK)
                            {
                                action = action | 128;
                            }
                            mount = *((unsigned short*) (character_data+164));
                            if(mount < MAX_CHARACTER)
                            {
                                action = action | 128;
                            }
                            packet_add_unsigned_char(action);                   // Action (high bit used if extra character data is to be sent...)
                            misc = character_data[78]<<6;
                            if((*((unsigned short*) (character_data+40))) > 0)
                            {
                                misc = misc | 32;
                            }
                            if((*((unsigned short*) (character_data+42))) > 0)
                            {
                                misc = misc | 16;
                            }
                            if(character_data[79] < 128)
                            {
                                misc = misc | 8;
                            }
                            if(character_data[216] & ENCHANT_FLAG_DEFLECT)
                            {
                                misc = misc | 4;
                            }
                            if(character_data[216] & ENCHANT_FLAG_HASTE)
                            {
                                misc = misc | 2;
                            }
                            if(character_data[216] & (ENCHANT_FLAG_SUMMON_3 | ENCHANT_FLAG_LEVITATE | ENCHANT_FLAG_INVISIBLE | ENCHANT_FLAG_MORPH))
                            {
                                misc = misc | 1;
                            }
                            packet_add_unsigned_char(misc);                     // Miscellaneous (top 2 bits are team) (then 1 bit for poison) (then 1 bit for petrify) (then 1 bit for low alpha) (then 1 bit for enchant_deflect) (then 1 bit for enchant_haste) (then 1 bit if enchanted in any way other than deflect & haste)
                            packet_add_unsigned_char(character_data[242]);      // EqLeft
                            packet_add_unsigned_char(character_data[243]);      // EqRight
                            packet_add_unsigned_char(character_data[240]);      // EqCol01
                            if(action & 128)
                            {
                                // Character is a high-data character...  Extra character data is to be sent...
                                packet_add_unsigned_char(character_data[241]);      // EqCol23
                                packet_add_unsigned_char(character_data[244]);      // EqSpec1
                                packet_add_unsigned_char(character_data[245]);      // EqSpec2
                                packet_add_unsigned_char(character_data[246]);      // EqHelm
                                packet_add_unsigned_char(character_data[247]);      // EqBody
                                packet_add_unsigned_char(character_data[248]);      // EqLegs
                                packet_add_unsigned_char(character_data[204]);      // Character class
                                if(mount < MAX_CHARACTER)
                                {
                                    packet_add_unsigned_char(mount);                    // Local index number of mount
                                }
                                else
                                {
                                    packet_add_unsigned_char(i);                        // Mount is not valid, so send our own local index number again (since we're obviously not riding ourself)
                                }
                            }
                            local_character_count--;
                        }
                    }
                }
                i++;
            }
        packet_end();



        network_send(NETWORK_ALL_REMOTES_IN_ROOM);
    }
}

//-----------------------------------------------------------------------------------------------
void network_send_ready(void)
{
    // <ZZ> Non-host player signals they are ready
    if(!network_on || !main_game_active) return;

    packet_begin(PACKET_TYPE_PLAYER_READY);
    packet_end();
    network_send(NETWORK_ALL_REMOTES_IN_GAME);
    join_state = 5;  // Ready
    log_message("INFO:   Sent PLAYER_READY");
}

//-----------------------------------------------------------------------------------------------
void network_send_start_game(void)
{
    // <ZZ> Host tells all remotes the game is starting
    if(!network_on || !lan_hosting) return;

    packet_begin(PACKET_TYPE_START_GAME);
    packet_end();
    network_send(NETWORK_ALL_REMOTES_IN_GAME);
    log_message("INFO:   Sent START_GAME to all remotes");
}

//-----------------------------------------------------------------------------------------------
void network_lan_broadcast(void)
{
    // <ZZ> Sends a LAN announcement packet via broadcast so other machines on the
    //      local network can discover this game.
    UDPpacket udp_packet;

    if(network_on && lan_hosting && lan_broadcast_socket)
    {
        packet_begin(PACKET_TYPE_LAN_ANNOUNCE);
            packet_add_unsigned_short(num_remote + 1);  // Player count (remotes + self)
            packet_add_unsigned_short(UDP_PORT);         // Game port to connect to
        packet_end();

        udp_packet.channel = -1;
        udp_packet.data = packet_buffer;
        udp_packet.len = packet_length;
        udp_packet.maxlen = MAX_PACKET_SIZE;
        udp_packet.address.host = 0xFFFFFFFF;  // Broadcast address
        #ifdef LIL_ENDIAN
            udp_packet.address.port = (LAN_BROADCAST_PORT>>8) | ((LAN_BROADCAST_PORT&255)<<8);
        #else
            udp_packet.address.port = LAN_BROADCAST_PORT;
        #endif
        SDLNet_UDP_Send(lan_broadcast_socket, -1, &udp_packet);
    }
}

//-----------------------------------------------------------------------------------------------
void network_lan_query(void)
{
    // <ZZ> Sends a LAN query to find games on the local network.
    UDPpacket udp_packet;

    if(network_on && lan_broadcast_socket)
    {
        packet_begin(PACKET_TYPE_LAN_QUERY);
        packet_end();

        udp_packet.channel = -1;
        udp_packet.data = packet_buffer;
        udp_packet.len = packet_length;
        udp_packet.maxlen = MAX_PACKET_SIZE;
        udp_packet.address.host = 0xFFFFFFFF;  // Broadcast address
        #ifdef LIL_ENDIAN
            udp_packet.address.port = (LAN_BROADCAST_PORT>>8) | ((LAN_BROADCAST_PORT&255)<<8);
        #else
            udp_packet.address.port = LAN_BROADCAST_PORT;
        #endif
        SDLNet_UDP_Send(lan_broadcast_socket, -1, &udp_packet);
    }
}

//-----------------------------------------------------------------------------------------------
void network_lan_listen(void)
{
    // <ZZ> Checks for LAN broadcast packets (announcements and queries).
    UDPpacket udp_packet;
    unsigned short i, player_count, game_port;

    if(!network_on || !lan_broadcast_socket) return;

    udp_packet.channel = -1;
    udp_packet.data = packet_buffer;
    udp_packet.len = MAX_PACKET_SIZE;
    udp_packet.maxlen = MAX_PACKET_SIZE;

    while(SDLNet_UDP_Recv(lan_broadcast_socket, &udp_packet))
    {
        packet_length = udp_packet.len;
        packet_decrypt();
        if(packet_valid())
        {
            if(packet_buffer[0] == PACKET_TYPE_LAN_ANNOUNCE)
            {
                // Someone is announcing their game
                packet_readpos = PACKET_HEADER_SIZE;
                packet_read_unsigned_short(player_count);
                packet_read_unsigned_short(game_port);

                // Skip our own broadcasts
                if(udp_packet.address.host == local_address.host) continue;

                // Check if we already know about this game
                for(i = 0; i < MAX_LAN_GAMES; i++)
                {
                    if(lan_game_on[i] && lan_game_address[i].host == udp_packet.address.host)
                    {
                        // Update existing entry
                        lan_game_players[i] = player_count;
                        lan_game_timeout[i] = 600;  // ~10 seconds at 60fps
                        goto next_lan_packet;
                    }
                }
                // Add new game to list
                for(i = 0; i < MAX_LAN_GAMES; i++)
                {
                    if(!lan_game_on[i])
                    {
                        lan_game_address[i].host = udp_packet.address.host;
                        lan_game_address[i].port = game_port;
                        lan_game_players[i] = player_count;
                        lan_game_on[i] = TRUE;
                        lan_game_timeout[i] = 600;
                        num_lan_games++;
                        log_message("INFO:   Found LAN game at %d.%d.%d.%d with %d players",
                            ((unsigned char*)&udp_packet.address.host)[0],
                            ((unsigned char*)&udp_packet.address.host)[1],
                            ((unsigned char*)&udp_packet.address.host)[2],
                            ((unsigned char*)&udp_packet.address.host)[3],
                            player_count);
                        break;
                    }
                }
            }
            else if(packet_buffer[0] == PACKET_TYPE_LAN_QUERY && lan_hosting)
            {
                // Someone is looking for games - reply directly to the querier
                UDPpacket reply_pkt;
                packet_begin(PACKET_TYPE_LAN_ANNOUNCE);
                    packet_add_unsigned_short(num_remote + 1);
                    packet_add_unsigned_short(UDP_PORT);
                packet_end();
                reply_pkt.channel = -1;
                reply_pkt.data = packet_buffer;
                reply_pkt.len = packet_length;
                reply_pkt.maxlen = MAX_PACKET_SIZE;
                reply_pkt.address = udp_packet.address;
                SDLNet_UDP_Send(lan_broadcast_socket, -1, &reply_pkt);
            }
        }
        next_lan_packet:;
    }

    // Tick down timeouts
    for(i = 0; i < MAX_LAN_GAMES; i++)
    {
        if(lan_game_on[i])
        {
            if(lan_game_timeout[i] > 0)
            {
                lan_game_timeout[i]--;
            }
            else
            {
                lan_game_on[i] = FALSE;
                num_lan_games--;
            }
        }
    }

    // Periodic broadcast if hosting
    if(lan_hosting)
    {
        if(lan_broadcast_timer > 0)
        {
            lan_broadcast_timer--;
        }
        else
        {
            network_lan_broadcast();
            lan_broadcast_timer = LAN_BROADCAST_INTERVAL;
        }
    }
}

//-----------------------------------------------------------------------------------------------
void network_join_game(unsigned char* ip_string)
{
    // <ZZ> Initiates joining a game at the given IP address
    UDPpacket udp_packet;

    if(!network_on) return;

    log_message("INFO:   Attempting to join game at %s", ip_string);

    if(SDLNet_ResolveHost(&join_target_address, ip_string, UDP_PORT) == 0)
    {
        // Send I_WANNA_PLAY packet
        packet_begin(PACKET_TYPE_I_WANNA_PLAY);
            packet_add_unsigned_int(game_seed);
        packet_end();

        udp_packet.channel = -1;
        udp_packet.data = packet_buffer;
        udp_packet.len = packet_length;
        udp_packet.maxlen = MAX_PACKET_SIZE;
        udp_packet.address.host = join_target_address.host;
        #ifdef LIL_ENDIAN
            udp_packet.address.port = (UDP_PORT>>8) | ((UDP_PORT&255)<<8);
        #else
            udp_packet.address.port = UDP_PORT;
        #endif
        SDLNet_UDP_Send(remote_socket, -1, &udp_packet);
        join_state = 1;  // Sent request
        join_timer = 15*60;  // 15 second timeout
        log_message("INFO:   Sent I_WANNA_PLAY to %d.%d.%d.%d",
            ((unsigned char*)&join_target_address.host)[0],
            ((unsigned char*)&join_target_address.host)[1],
            ((unsigned char*)&join_target_address.host)[2],
            ((unsigned char*)&join_target_address.host)[3]);
    }
    else
    {
        log_message("ERROR:  Could not resolve host %s", ip_string);
        join_state = 0;
    }
}

//-----------------------------------------------------------------------------------------------
void network_join_game_ip(IPaddress* addr)
{
    // <ZZ> Initiates joining a game at the given IP address struct (for LAN games)
    UDPpacket udp_packet;

    if(!network_on) return;

    join_target_address = *addr;

    // Send I_WANNA_PLAY packet
    packet_begin(PACKET_TYPE_I_WANNA_PLAY);
        packet_add_unsigned_int(game_seed);
    packet_end();

    udp_packet.channel = -1;
    udp_packet.data = packet_buffer;
    udp_packet.len = packet_length;
    udp_packet.maxlen = MAX_PACKET_SIZE;
    udp_packet.address.host = join_target_address.host;
    #ifdef LIL_ENDIAN
        udp_packet.address.port = (UDP_PORT>>8) | ((UDP_PORT&255)<<8);
    #else
        udp_packet.address.port = UDP_PORT;
    #endif
    SDLNet_UDP_Send(remote_socket, -1, &udp_packet);
    join_state = 1;
    join_timer = 15*60;
    log_message("INFO:   Sent I_WANNA_PLAY to %d.%d.%d.%d",
        ((unsigned char*)&join_target_address.host)[0],
        ((unsigned char*)&join_target_address.host)[1],
        ((unsigned char*)&join_target_address.host)[2],
        ((unsigned char*)&join_target_address.host)[3]);
}

//-----------------------------------------------------------------------------------------------
void network_host_game(void)
{
    // <ZZ> Start hosting a game
    if(!network_on) return;

    lan_hosting = TRUE;
    lan_broadcast_timer = 0;  // Broadcast immediately
    main_game_active = TRUE;
    log_message("INFO:   Now hosting a game");
    network_lan_broadcast();

    // Also register with master server if available
    if(main_server_on)
    {
        packet_begin(PACKET_TYPE_REQUEST_JOIN);
            packet_add_unsigned_short(EXECUTABLE_VERSION_NUMBER);
            packet_add_unsigned_short(DATA_VERSION_NUMBER);
            packet_add_unsigned_char(0);    // continent
            packet_add_unsigned_char(0);    // direction
            packet_add_unsigned_char(0);    // letter
            packet_add_unsigned_char(1);    // number of players
            // Send LAN IP so server can detect same-network players
            {
                unsigned char* lip = (unsigned char*)&local_address.host;
                packet_add_unsigned_char(lip[0]);
                packet_add_unsigned_char(lip[1]);
                packet_add_unsigned_char(lip[2]);
                packet_add_unsigned_char(lip[3]);
            }
        packet_end_plain();
        network_server_send();
        log_message("INFO:   Registered with master server");
    }
}


//-----------------------------------------------------------------------------------------------
// ServerFu master server functions
//-----------------------------------------------------------------------------------------------
void network_request_shard_list(void)
{
    // <ZZ> Request the list of available shards from the master server
    if(!network_on || !main_server_on) return;

    packet_begin(PACKET_TYPE_REQUEST_SHARD_LIST);
        packet_add_unsigned_short(EXECUTABLE_VERSION_NUMBER);
        packet_add_unsigned_short(DATA_VERSION_NUMBER);
        packet_add_unsigned_char(0);    // continent
        packet_add_unsigned_char(0);    // direction
    packet_end_plain();
    network_server_send();
    log_message("INFO:   Sent shard list request to master server");
}

//-----------------------------------------------------------------------------------------------
void network_request_player_count(void)
{
    // <ZZ> Request the total player count from the master server
    if(!network_on || !main_server_on) return;

    packet_begin(PACKET_TYPE_REQUEST_PLAYER_COUNT);
    packet_end_plain();
    network_server_send();
    log_message("INFO:   Sent player count request to master server");
}

//-----------------------------------------------------------------------------------------------
void network_request_join_server(void)
{
    // <ZZ> Request to join a game via the master server (ServerFu join flow)
    if(!network_on || !main_server_on) return;

    packet_begin(PACKET_TYPE_REQUEST_JOIN);
        packet_add_unsigned_short(EXECUTABLE_VERSION_NUMBER);
        packet_add_unsigned_short(DATA_VERSION_NUMBER);
        packet_add_unsigned_char(0);    // continent
        packet_add_unsigned_char(0);    // direction
        packet_add_unsigned_char(0);    // letter
        packet_add_unsigned_char(1);    // number of players
        // Send LAN IP so server can detect same-network players
        {
            unsigned char* lip = (unsigned char*)&local_address.host;
            packet_add_unsigned_char(lip[0]);
            packet_add_unsigned_char(lip[1]);
            packet_add_unsigned_char(lip[2]);
            packet_add_unsigned_char(lip[3]);
        }
    packet_end_plain();
    network_server_send();
    join_state = 1;
    join_timer = 15*60;  // 15 second timeout
    log_message("INFO:   Sent join request to master server");
}

//-----------------------------------------------------------------------------------------------
void network_send_heartbeat_server(void)
{
    // <ZZ> Send a heartbeat to the master server to stay registered
    if(!network_on || !main_server_on || !main_game_active) return;

    packet_begin(PACKET_TYPE_HEARTBEAT);
    packet_end_plain();
    network_server_send();
}

//-----------------------------------------------------------------------------------------------
void network_report_position_server(unsigned char room_x, unsigned char room_y, unsigned char room_z)
{
    // <ZZ> Report our current room position to the master server
    if(!network_on || !main_server_on || !main_game_active) return;

    packet_begin(PACKET_TYPE_REPORT_POSITION);
        packet_add_unsigned_char(room_x);
        packet_add_unsigned_char(room_y);
        packet_add_unsigned_char(room_z);
    packet_end_plain();
    network_server_send();
}

//-----------------------------------------------------------------------------------------------
void network_report_machine_down_server(unsigned int down_ip)
{
    // <ZZ> Report that a machine has disconnected to the master server
    if(!network_on || !main_server_on) return;

    packet_begin(PACKET_TYPE_REPORT_MACHINE_DOWN);
        packet_add_unsigned_char(0);    // continent
        packet_add_unsigned_char(0);    // direction
        packet_add_unsigned_char(0);    // letter
        packet_add_unsigned_int(down_ip);
    packet_end_plain();
    network_server_send();
}

//-----------------------------------------------------------------------------------------------
void network_leave_server(unsigned short minutes_played)
{
    // <ZZ> Tell the master server we're leaving the game
    if(!network_on || !main_server_on) return;

    packet_begin(PACKET_TYPE_REPORT_MACHINE_DOWN);
        packet_add_unsigned_char(0);    // continent
        packet_add_unsigned_char(0);    // direction
        packet_add_unsigned_char(0);    // letter
        packet_add_unsigned_int(0);     // ip=0.0.0.0 means self-report
        packet_add_unsigned_short(minutes_played);
    packet_end_plain();
    network_server_send();
    log_message("INFO:   Reported leaving to master server (%d minutes played)", minutes_played);
}

//-----------------------------------------------------------------------------------------------
void network_leave_game(void)
{
    // <ZZ> Leave the current game, notifying all remotes and cleaning up
    unsigned short i;

    if(!network_on) return;
    if(!main_game_active && !lan_hosting && num_remote == 0) return;

    log_message("INFO:   Leaving game...");

    // Send I_AM_HERE with 0 characters as a "goodbye" (remotes will time us out)
    // Also send a room update with 0 characters so remotes kill our characters immediately
    if(num_remote > 0)
    {
        packet_begin(PACKET_TYPE_ROOM_UPDATE);
            packet_add_unsigned_short(map_current_room);
            packet_add_unsigned_short(0);       // seed
            packet_add_unsigned_char(0);        // door flags
            packet_add_unsigned_char(0);        // 0 characters = we're leaving
        packet_end();
        network_send(NETWORK_ALL_REMOTES_IN_GAME);
    }

    // Clean up all remote characters locally
    repeat(i, MAX_REMOTE)
    {
        if(remote_on[i])
        {
            network_disconnect_remote(i);
        }
    }

    // Report to master server if connected
    network_leave_server(0);

    // Reset state
    lan_hosting = FALSE;
    main_game_active = FALSE;
    join_state = 0;
    num_remote = 0;
}

//-----------------------------------------------------------------------------------------------
void network_server_tick(void)
{
    // <ZZ> Called every frame to handle master server periodic tasks (heartbeat)
    if(!network_on || !main_server_on || !main_game_active) return;

    if(server_heartbeat_timer > 0)
    {
        server_heartbeat_timer--;
    }
    else
    {
        network_send_heartbeat_server();
        server_heartbeat_timer = SERVER_HEARTBEAT_INTERVAL;
    }
}

//-----------------------------------------------------------------------------------------------
