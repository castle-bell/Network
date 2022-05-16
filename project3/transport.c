/*
 * transport.c 
 *
 * CS244a HW#3 (Reliable Transport)
 *
 * This file implements the STCP layer that sits between the
 * mysocket and network layers. You are required to fill in the STCP
 * functionality in this file. 
 *
 */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"
#include "mysock_impl.h"


enum { CSTATE_LISTEN, CSTATE_SYN_SENT, CSTATE_SYN_RCVD, CSTATE_ESTABLISHED,
        CSTATE_FIN_WAIT_1, CSTATE_FIN_WAIT_2, CSTATE_TIME_WAIT,
        CSTATE_CLOSE_WAIT, CSTATE_LAST_ACK, CSTATE_CLOSED, CSTATE_CLOSING };    /* obviously you should have more states */

#define STCP_MSS 536

/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;

    /* any other connection-wide global variables go here */
    tcp_seq current_sequence_num;
    tcp_seq current_acknowledge_num;
    tcp_seq prev_sequence_num;

} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void generate_initial_ack_num(context_t *ctx);
static void generate_initialize_seq_num(context_t *ctx);
static void set_seq_num(context_t *ctx, uint32_t data_len);
static tcp_seq get_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);

static void copy(char *src, char *dest, int size);
static char *make_packet(context_t *ctx, char *data, int start, int size);
static void FSM_three_way_hs(mysocket_t sd, context_t *ctx);
static void FSM_four_way_hs(mysocket_t sd, context_t *ctx);

/* initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
    context_t *ctx;

    ctx = (context_t *) calloc(1, sizeof(context_t));
    assert(ctx);

    generate_initial_seq_num(ctx);
    generate_initial_ack_num(ctx);
    generate_initialize_seq_num(ctx);

    /* XXX: you should send a SYN packet here if is_active, or wait for one
     * to arrive if !is_active.  after the handshake completes, unblock the
     * application with stcp_unblock_application(sd).  you may also use
     * this to communicate an error condition back to the application, e.g.
     * if connection fails; to do so, just set errno appropriately (e.g. to
     * ECONNREFUSED, etc.) before calling the function.
     */

    /* Set initial state of context */
    if(is_active)
        ctx->connection_state = CSTATE_CLOSED;
    else
        ctx->connection_state = CSTATE_LISTEN;

    /* Implement three way handshaking */
    FSM_three_way_hs(sd, ctx);

    stcp_unblock_application(sd);

    control_loop(sd, ctx);

    /* do any cleanup here */
    free(ctx);
}


/* generate initial sequence number for an STCP connection */
static void generate_initial_seq_num(context_t *ctx)
{
    assert(ctx);
    ctx->initial_sequence_num = 1;
}

static void generate_initialize_seq_num(context_t *ctx)
{
    assert(ctx);
    ctx->current_sequence_num = ctx->initial_sequence_num;
}

static void generate_initial_ack_num(context_t *ctx)
{
    assert(ctx);
    ctx->current_acknowledge_num = 0;
}

/* Set sequence number of ctx */
static void set_seq_num(context_t *ctx, uint32_t data_len)
{
    assert(ctx);
    assert(data_len >= sizeof(STCPHeader));

    ctx->current_sequence_num = (ctx->current_sequence_num + data_len - sizeof(STCPHeader))%(__UINT32_MAX__);
}

/* Set acknowledge number of ctx */
static void set_ack_num(context_t *ctx, uint32_t data_len)
{
    assert(ctx);
    assert(data_len >= sizeof(STCPHeader));

    ctx->current_acknowledge_num = (ctx->current_acknowledge_num + data_len - sizeof(STCPHeader))%(__UINT32_MAX__);
}

/* Get sequence number for an STCP connection in Network endian */
static tcp_seq get_seq_num(context_t *ctx)
{
    assert(ctx);
    return htonl(ctx->current_sequence_num);
}

/* Get acknowledge number for an STCP connection in Network endian */
static tcp_seq get_ack_num(context_t *ctx)
{
    assert(ctx);
    return htonl(ctx->current_acknowledge_num);
}

/* control_loop() is the main STCP loop; it repeatedly waits for one of the
 * following to happen:
 *   - incoming data from the peer
 *   - new data from the application (via mywrite())
 *   - the socket to be closed (via myclose())
 *   - a timeout
 */
static void control_loop(mysocket_t sd, context_t *ctx)
{
    assert(ctx);
    assert(ctx->connection_state == CSTATE_ESTABLISHED);

    /* Use prev_sequence_num element */
    ctx->prev_sequence_num = ctx->current_sequence_num;

    /* Parameter for data transmission */
    int max_size = 2952; /* When window size is 3072, this value is the amount of payload to get */
    int size = 0; /* # elements in sender_buffer */
    int unacked = 0; /* # unacked elements in sender_buffer */
    char sender_buffer[2952];

    /* Get app socket */
    mysock_context_t *ctx_app = _mysock_get_context(sd);
    mysocket_t app = ctx_app->my_sd;

    while (!ctx->done)
    {
        unsigned int event;
        /* see stcp_api.h or stcp_api.c for details of this function */
        /* XXX: you will need to change some of these arguments! */
        event = stcp_wait_for_event(sd, ANY_EVENT, NULL);

        char recv_buffer[STCP_MSS + sizeof(STCPHeader)];
        STCPHeader send_header;
        STCPHeader recv_header;
        uint32_t recv = 0;


        /* check whether it was the network, app, or a close request */
        if (event & APP_DATA)
        {
            /* the application has requested that data be sent */
            /* see stcp_app_recv() */

            /* Get the data from application layer */
            if(max_size > size && (unacked == 0))
            {
                size += stcp_app_recv(app, sender_buffer + size, max_size - size);
                assert(size <= max_size);

                while(unacked < size)
                {
                    int s_size = ((size - unacked) > STCP_MSS) ? STCP_MSS : (size - unacked); /* Size of payload to send */
                    char *packet = make_packet(ctx, sender_buffer, unacked, s_size);
                    set_seq_num(ctx, stcp_network_send(sd,packet,(s_size + sizeof(STCPHeader)), NULL));
                    /* Set the unacked properly */
                    unacked += s_size;
                    free(packet);
                }
                assert(unacked <= size);
            }
        }
        if (event & NETWORK_DATA)
        {
            /* There are 2 cases 
               1) Receive FINACK packet
               2) Receive data packet
            */
           recv = stcp_network_recv(sd,recv_buffer,sizeof(recv_buffer));
           copy(recv_buffer, (char *)&recv_header, sizeof(STCPHeader));

            /* Case of server got FINACK packet */
           if(recv_header.th_flags == (TH_FIN | TH_ACK)) /* Case 1 */
            {
                set_ack_num(ctx, recv+1);
                send_header.th_seq = get_seq_num(ctx);
                send_header.th_ack = get_ack_num(ctx); /* Useless, but initialize */
                send_header.th_flags = TH_ACK;
                send_header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd, &send_header, sizeof(send_header), NULL));
                ctx->connection_state = CSTATE_CLOSE_WAIT;
                stcp_fin_received(sd);
                // FSM_four_way_hs(sd, ctx);
            }

            /* Case of server get data packet */
            else if(recv_header.th_flags == TH_ACK)
            {
                if(recv == sizeof(STCPHeader))
                {
                    /* Resize the rearrange the buffer */
                    int acked = ntohl(recv_header.th_ack) - ctx->prev_sequence_num;
                    ctx->prev_sequence_num = ntohl(recv_header.th_ack);
                    unacked -= acked;
                    size -= acked;
                    copy(sender_buffer+acked, sender_buffer, size);
                    memset(sender_buffer+size, 0, max_size-size);
                    continue;
                }

                /* Send ack to peer */
                set_ack_num(ctx, recv);
                send_header.th_seq = get_seq_num(ctx);
                send_header.th_ack = get_ack_num(ctx);
                send_header.th_flags = TH_ACK;
                send_header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd, &send_header, sizeof(send_header), NULL));

                /* Send payload to application */
                char payload[recv - sizeof(STCPHeader)];
                copy(recv_buffer+sizeof(STCPHeader), payload, recv-sizeof(STCPHeader));
                stcp_app_send(app, payload, sizeof(payload));
            }

            else
            {
                printf("Strange packet\n");
                assert(0);
            }
        }
        if (event & APP_CLOSE_REQUESTED)
        {
            FSM_four_way_hs(sd, ctx);
        }
        /* etc. */
    }
}


/**********************************************************************/
/* our_dprintf
 *
 * Send a formatted message to stdout.
 * 
 * format               A printf-style format string.
 *
 * This function is equivalent to a printf, but may be
 * changed to log errors to a file if desired.
 *
 * Calls to this function are generated by the dprintf amd
 * dperror macros in transport.h
 */
void our_dprintf(const char *format,...)
{
    va_list argptr;
    char buffer[1024];

    assert(format);
    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    fputs(buffer, stdout);
    fflush(stdout);
}

static void copy(char *src, char *dest, int size)
{
    for(int i = 0; i<size; i++)
        dest[i] = src[i];
}

static char *make_packet(context_t *ctx, char *data, int start, int size)
{
    char *packet = calloc((STCP_MSS + sizeof(STCPHeader)), sizeof(char));
    STCPHeader header;

    /* Copy the header into packet */
    header.th_seq = get_seq_num(ctx);
    header.th_ack = get_ack_num(ctx); /* Useless, but initialize */
    header.th_flags = TH_ACK;
    header.th_win = htons(3072);
    copy((char*)(&header),packet,sizeof(STCPHeader));

    /* Copy the data into packet */
    copy(data+start, packet+sizeof(STCPHeader), size);

    return packet;
}

static void FSM_three_way_hs(mysocket_t sd, context_t *ctx)
{
    assert(ctx);

    STCPHeader header;
    uint32_t recv;

    while(ctx->connection_state != CSTATE_ESTABLISHED)
    {
        STCPHeader recv_header;
        switch(ctx->connection_state)
        {
            /* It means start active open */
            case CSTATE_CLOSED:
                header.th_seq = get_seq_num(ctx);
                header.th_ack = get_ack_num(ctx); /* Useless, but initialize */
                header.th_flags = TH_SYN;
                header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd, &header, sizeof(header), NULL)+1);
                ctx->connection_state = CSTATE_SYN_SENT;
                break;
            /* It meaans start passive open */
            case CSTATE_LISTEN:
                recv = stcp_network_recv(sd,&recv_header, sizeof(recv_header));
            
                /* Check whether the flag is SYN */
                if(recv_header.th_flags != TH_SYN)
                    continue;

                set_ack_num(ctx, recv+2); /* 1->2 because of initialization */
                /* Send SYNACK meg to client */
                header.th_seq = get_seq_num(ctx);
                header.th_ack = get_ack_num(ctx);
                header.th_flags = (TH_ACK | TH_SYN);
                header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd, &header, sizeof(header), NULL)+1);
                ctx->connection_state = CSTATE_SYN_RCVD;
                break;
            case CSTATE_SYN_SENT:
                /* Get the ACK packet */
                recv = stcp_network_recv(sd,&recv_header,sizeof(recv_header));
                if(recv_header.th_flags != (TH_ACK | TH_SYN))
                    continue;
                set_ack_num(ctx, recv+2); /* 1->2 because of initialization */
                /* Send the ACK packet */
                /* 나중에 저장해둔 seq_num이랑 받은 message의 ack랑 같은지 확인 */
                header.th_seq = get_seq_num(ctx); /* It is already set to network endian */
                header.th_ack = get_ack_num(ctx);
                header.th_flags = TH_ACK;
                header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd,&header,sizeof(header),NULL));
                ctx->connection_state = CSTATE_ESTABLISHED;
                break;

            case CSTATE_SYN_RCVD:
                /* Get the ACK packet */
                recv = stcp_network_recv(sd,&recv_header,sizeof(recv_header)); /* It may contains the message */
                /* But now I assume there is no data, I implement that later */
                if(recv_header.th_flags != TH_ACK)
                    continue;
                set_ack_num(ctx, recv);
                ctx->connection_state = CSTATE_ESTABLISHED;
                /* Implement something here */
                break;
        }
    }
}


/* Implement FSM of 4-way hand shaking. Event implies
   1) Close system call is occurred, if event = 0.
   2) FIN packet is received, if event = 1.
   3) Simultaneous close, if event = 2. */
static void FSM_four_way_hs(mysocket_t sd, context_t *ctx)
{
    STCPHeader header;
    STCPHeader recv_header;
    uint32_t recv = 0;
    while(!ctx->done)
    {
        switch(ctx->connection_state)
        {
            case CSTATE_ESTABLISHED:
                header.th_seq = get_seq_num(ctx);
                header.th_ack = get_ack_num(ctx);
                header.th_flags = (TH_FIN | TH_ACK);
                header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd, &header, sizeof(header), NULL) + 1);
                ctx->connection_state = CSTATE_FIN_WAIT_1;
                break;

            case CSTATE_FIN_WAIT_1:
                /* Get the ACK packet */
                recv = stcp_network_recv(sd,&recv_header,sizeof(recv_header));
                if(recv_header.th_flags != TH_ACK && recv_header.th_flags != (TH_FIN | TH_ACK))
                    printf("Undefined behavior\n");
                if(recv_header.th_flags == TH_ACK)
                {
                    set_ack_num(ctx, recv);
                    ctx->connection_state = CSTATE_FIN_WAIT_2;
                }
                else /* FIN ACK(simultaneous close) */
                {
                    set_ack_num(ctx, recv+1);
                    header.th_seq = get_seq_num(ctx);
                    header.th_ack = get_ack_num(ctx);
                    header.th_flags = TH_ACK;
                    header.th_win = htons(3072);
                    set_seq_num(ctx, stcp_network_send(sd, &header, sizeof(header), NULL));
                    ctx->connection_state = CSTATE_CLOSING;
                }
                break;

            case CSTATE_FIN_WAIT_2:
                recv = stcp_network_recv(sd,&recv_header,sizeof(recv_header));
                if(recv_header.th_flags != (TH_ACK | TH_FIN))
                    printf("Undefined behavior\n");
                set_ack_num(ctx, recv+1);
                header.th_seq = get_seq_num(ctx);
                header.th_ack = get_ack_num(ctx);
                header.th_flags = TH_ACK;
                header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd, &header, sizeof(header), NULL));
                ctx->connection_state = CSTATE_TIME_WAIT;
                stcp_fin_received(sd);
                break;

            case CSTATE_TIME_WAIT:
                ctx->connection_state = CSTATE_CLOSED;
                ctx->done = 1;
                break;

            case CSTATE_CLOSE_WAIT:
                header.th_seq = get_seq_num(ctx);
                header.th_ack = get_ack_num(ctx);
                header.th_flags = (TH_FIN | TH_ACK);
                header.th_win = htons(3072);
                set_seq_num(ctx, stcp_network_send(sd, &header, sizeof(header), NULL) + 1);
                ctx->connection_state = CSTATE_LAST_ACK;
                break;
            
            case CSTATE_LAST_ACK:
                recv = stcp_network_recv(sd,&recv_header,sizeof(recv_header));
                set_ack_num(ctx, recv);
                ctx->connection_state = CSTATE_CLOSED;
                ctx->done = 1;
                break;

            case CSTATE_CLOSING:
                recv = stcp_network_recv(sd,&recv_header,sizeof(recv_header));
                if(recv_header.th_flags != TH_ACK)
                    printf("Undefined behavior\n");
                set_ack_num(ctx, recv);
                ctx->connection_state = CSTATE_TIME_WAIT;
                break;

        }
    }

}