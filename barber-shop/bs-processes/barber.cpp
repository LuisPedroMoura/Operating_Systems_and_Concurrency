#include <stdlib.h>
#include "dbc.h"
#include "global.h"
#include "utils.h"
#include "box.h"
#include "timer.h"
#include "logger.h"
#include "barber-shop.h"
#include "barber.h"

enum BCState
{
   NO_BARBER_GREET,           //barber has yet to receive and greet the client
   GREET_AVAILABLE,	      //client can get barberID
   WAITING_ON_RESERVE,        //client waiting until the barber has reserved the seat for the process
   RESERVED,                  //chair reserved
   SERVICE_INFO_AVAILABLE,    //client has been informed
   WAITING_ON_CLIENT_SIT,     //barber waiting on client to sit
   CLIENT_SEATED,	      //client has sat down
   PROCESSING,		      //process started
   WAITING_ON_CLIENT_RISE,    //barber waiting for client to leave the spot
   CLIENT_RISEN,              //client left the spot
   PROCESS_DONE,              //process has finished
   ALL_PROCESSES_DONE         //all processes done   
};

enum State
{
   NONE = 0,
   CUTTING,
   SHAVING,
   WASHING,
   WAITING_CLIENTS,
   WAITING_BARBER_SEAT,
   WAITING_WASHBASIN,
   REQ_SCISSOR,
   REQ_COMB,
   REQ_RAZOR,
   DONE,
};

#define State_SIZE (DONE - NONE + 1)

static const char* stateText[State_SIZE] =
{
   "---------",
   "CUTTING  ",
   "SHAVING  ",
   "WASHING  ",
   "W CLIENT ", // Waiting for client
   "W SEAT   ", // Waiting for barber seat
   "W BASIN  ", // Waiting for washbasin
   "R SCISSOR", // Request a scissor
   "R COMB   ", // Request a comb
   "R RAZOR  ", // Request a razor
   "DONE     ",
};

static const char* skel = 
   "@---+---+---@\n"
   "|B##|C##|###|\n"
   "+---+---+-+-+\n"
   "|#########|#|\n"
   "@---------+-@";
static int skel_length = num_lines_barber()*(num_columns_barber()+1)*4; // extra space for (pessimistic) utf8 encoding!

static void life(Barber* barber);

static void sit_in_barber_bench(Barber* barber);
static void wait_for_client(Barber* barber);
static int work_available(Barber* barber);
static void rise_from_barber_bench(Barber* barber);
static void process_resquests_from_client(Barber* barber);
static void release_client(Barber* barber);
static void done(Barber* barber);
static void process_haircut_request(Barber* barber);
static void process_hairwash_request(Barber* barber);
static void process_shave_request(Barber* barber);

static char* to_string_barber(Barber* barber);

size_t sizeof_barber()
{
   return sizeof(Barber);
}

int num_lines_barber()
{
   return string_num_lines((char*)skel);
}

int num_columns_barber()
{
   return string_num_columns((char*)skel);
}

void init_barber(Barber* barber, int id, BarberShop* shop, int line, int column)
{
   require (barber != NULL, "barber argument required");
   require (id > 0, concat_3str("invalid id (", int2str(id), ")"));
   require (shop != NULL, "barber shop argument required");
   require (line >= 0, concat_3str("Invalid line (", int2str(line), ")"));
   require (column >= 0, concat_3str("Invalid column (", int2str(column), ")"));

   barber->id = id;
   barber->state = NONE;
   barber->shop = shop;
   barber->clientID = 0;
   barber->reqToDo = 0;
   barber->benchPosition = -1;
   barber->chairPosition = -1;
   barber->basinPosition = -1;
   barber->tools = 0;
   barber->internal = NULL;
   barber->logId = register_logger((char*)("Barber:"), line ,column,
                                   num_lines_barber(), num_columns_barber(), NULL);
}

void term_barber(Barber* barber)
{
   require (barber != NULL, "barber argument required");

   if (barber->internal != NULL)
   {
      mem_free(barber->internal);
      barber->internal = NULL;
   }
}

void log_barber(Barber* barber)
{
   require (barber != NULL, "barber argument required");

   spend(random_int(global->MIN_VITALITY_TIME_UNITS, global->MAX_VITALITY_TIME_UNITS));
   send_log(barber->logId, to_string_barber(barber));
}

void* main_barber(void* args)
{
   Barber* barber = (Barber*)args;
   require (barber != NULL, "barber argument required");
   life(barber);
   bci_connect();
   return NULL;
}

static void life(Barber* barber)
{
   require (barber != NULL, "barber argument required");

   barber->shop->opened = 1;
   sit_in_barber_bench(barber);
   wait_for_client(barber);
   while(work_available(barber)) // no more possible clients and closes barbershop
   {
      rise_from_barber_bench(barber);
      process_resquests_from_client(barber);
      release_client(barber);
      sit_in_barber_bench(barber);
      wait_for_client(barber);
   }
   done(barber);
}

static void sit_in_barber_bench(Barber* barber)
{
   /** TODO:
    * 1: sit in a random empty seat in barber bench (always available)
    **/

   require (barber != NULL, "barber argument required");
   require (num_seats_available_barber_bench(barber_bench(barber->shop)) > 0, "seat not available in barber shop");
   require (!seated_in_barber_bench(barber_bench(barber->shop), barber->id), "barber already seated in barber shop");

   barber->benchPosition = random_sit_in_barber_bench(barber_bench(barber->shop),barber->id); 
   log_barber(barber);
   
   bci_set_state(barber->id,NO_BARBER_GREET);
}

static void wait_for_client(Barber* barber)
{
   /** TODO:
    * 1: set the barber state to WAITING_CLIENTS
    * 2: get next client from client benches (if empty, wait) (also, it may be required to check for simulation termination)
    * 3: receive and greet client (receive its requested services, and give back the barber's id)
    **/

   require (barber != NULL, "barber argument required");

   barber->state = WAITING_CLIENTS;
   log_barber(barber);
	 
   while(bci_get_num_clients_in_bench() == 0) {
     spend(2*global->MAX_OUTSIDE_TIME_UNITS);
     if(bci_get_num_clients_in_bench() == 0) close_shop(barber->shop);
   }
	 
   bci_get_syncBenches(client_benches(barber->shop));

   RQItem queue_item = next_client_in_benches(client_benches(barber->shop));
   RQItem* tmp_qitem = &(queue_item);

   receive_and_greet_client(barber->shop,barber->id,tmp_qitem->clientID);
 
   bci_set_state(barber->id,GREET_AVAILABLE);
 
   barber->clientID = tmp_qitem->clientID;
 
   bci_grant_client_access(barber->clientID);
 
   log_barber(barber);  // (if necessary) more than one in proper places!!!
}

static int work_available(Barber* barber)
{
   /** TODO:
    * 1: find a safe way to solve the problem of barber termination
    **/

   require (barber != NULL, "barber argument required");

   if(barber->clientID > 0)
     return 1;

   return 0;
}

static void rise_from_barber_bench(Barber* barber)
{
   /** TODO:
    * 1: rise from the seat of barber bench
    **/

   require (barber != NULL, "barber argument required");
   require (seated_in_barber_bench(barber_bench(barber->shop), barber->id), "barber not seated in barber shop");

   rise_barber_bench(barber_bench(barber->shop),barber->benchPosition);
   barber->benchPosition = -1;

   log_barber(barber);
}

static void process_resquests_from_client(Barber* barber)
{
   /** TODO:
    * Process one client request at a time, until all requests are fulfilled.
    * For each request:
    * 1: select the request to process (any order is acceptable)
    * 2: reserve the chair/basin for the service (setting the barber's state accordingly) 
    *    2.1: set the client state to a proper value
    *    2.2: reserve a random empty chair/basin 
    *    2.3: inform client on the service to be performed
    * 3: depending on the service, grab the necessary tools from the pot (if any)
    * 4: process the service (see [incomplete] process_haircut_request as an example)
    * 5: return the used tools to the pot (if any)
    *
    *
    * At the end the client must leave the barber shop
    **/

   require (barber != NULL, "barber argument required");

   while(bci_get_request(barber->clientID) > 0) {

     bci_set_state(barber->id,WAITING_ON_RESERVE);

     barber->state = WAITING_CLIENTS;
     log_barber(barber);

     barber->reqToDo = bci_get_next_request(barber->clientID);

     if(barber->reqToDo == 1) {
       while(num_available_barber_chairs(barber->shop) == 0);
       barber->chairPosition = reserve_random_empty_barber_chair(barber->shop,barber->id);
       BarberChair* tmp_bbc1 = barber_chair(barber->shop,barber->chairPosition);
       bci_set_syncBBChair(*tmp_bbc1,barber->id);
     }
     else if(barber->reqToDo == 2) {
       while(num_available_washbasin(barber->shop) == 0);
       barber->basinPosition = reserve_random_empty_washbasin(barber->shop,barber->id);
       Washbasin* tmp_wsh = washbasin(barber->shop,barber->basinPosition);
       bci_set_syncWashbasin(*tmp_wsh,barber->id);
     }
     else {
       while(num_available_barber_chairs(barber->shop) == 0);
       barber->chairPosition = reserve_random_empty_barber_chair(barber->shop,barber->id);
       BarberChair* tmp_bbc2 = barber_chair(barber->shop,barber->chairPosition);
       bci_set_syncBBChair(*tmp_bbc2,barber->id);
     }
 
     log_barber(barber); 
     bci_set_state(barber->id,RESERVED); 
      
     Service service_to_send; 
     if(barber->reqToDo == 1 || barber->reqToDo == 4)
       set_barber_chair_service(&service_to_send,barber->id,barber->clientID,barber->chairPosition,barber->reqToDo);
     else
       set_washbasin_service(&service_to_send,barber->id,barber->clientID,barber->basinPosition); 
      
     inform_client_on_service(barber->shop,service_to_send); 

     if(barber->reqToDo == 1) {
       barber->state = REQ_SCISSOR;
       log_barber(barber);
       while((tools_pot(barber->shop))->availScissors == 0);
       pick_scissor(tools_pot(barber->shop));
       barber->tools += 1;

       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchair1 = barber_chair(barber->shop,barber->chairPosition);
       bbchair1->toolsHolded += 1;
       bci_set_syncBBChair(*bbchair1,barber->id);

       barber->state = REQ_COMB;
       log_barber(barber);
       while((tools_pot(barber->shop))->availCombs == 0);
       pick_comb(tools_pot(barber->shop));
       barber->tools += 2;

       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchair2 = barber_chair(barber->shop,barber->chairPosition);
       bbchair2->toolsHolded += 2;
       bci_set_syncBBChair(*bbchair2,barber->id);
     }
     else if(barber->reqToDo == 4) {
       barber->state = REQ_RAZOR;
       log_barber(barber);
       while((tools_pot(barber->shop))->availRazors == 0);
       pick_razor(tools_pot(barber->shop));
       barber->tools += 4;

       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchair3 = barber_chair(barber->shop,barber->chairPosition);
       bbchair3->toolsHolded += 4;
       bci_set_syncBBChair(*bbchair3,barber->id);
     }
	 
     log_barber(barber);

     //WAIT FOR SIT
     if(bci_get_state(barber->id) < CLIENT_SEATED)
       bci_set_state(barber->id,WAITING_ON_CLIENT_SIT);
     while(bci_get_state(barber->id) < CLIENT_SEATED);
 
     bci_set_state(barber->id,PROCESSING);

     if(barber->reqToDo == 1) {
       barber->state = CUTTING;
       log_barber(barber);

       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchaircut= barber_chair(barber->shop,barber->chairPosition);
       process_haircut_request(barber);
       bci_set_syncBBChair(*bbchaircut,barber->id);
     }
     else if(barber->reqToDo == 2) {
       barber->state = WASHING;
       log_barber(barber);
       bci_get_syncWashbasin(washbasin(barber->shop,barber->basinPosition),barber->id);
       Washbasin* wshbasin = washbasin(barber->shop,barber->basinPosition);
       process_hairwash_request(barber);
       bci_set_syncWashbasin(*wshbasin,barber->id);
     }
     else {
       barber->state = SHAVING;
       log_barber(barber);
       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchairshave= barber_chair(barber->shop,barber->chairPosition);
       process_shave_request(barber);
       bci_set_syncBBChair(*bbchairshave,barber->id);
     }

     bci_set_state(barber->id,WAITING_ON_CLIENT_RISE);
     while(bci_get_state(barber->id) == WAITING_ON_CLIENT_RISE);
	 
     barber->state = DONE;

     if(barber->reqToDo == 1) {
       return_scissor(tools_pot(barber->shop));
       barber->tools -= 1;
       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchair4 = barber_chair(barber->shop,barber->chairPosition);
       bbchair4->toolsHolded -= 1;
       bci_set_syncBBChair(*bbchair4,barber->id);
       
       return_comb(tools_pot(barber->shop));
       barber->tools -= 2;
       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchair5 = barber_chair(barber->shop,barber->chairPosition);
       bbchair5->toolsHolded -= 2;
       bci_set_syncBBChair(*bbchair5,barber->id);
     }
     else if(barber->reqToDo == 4) {
       return_razor(tools_pot(barber->shop));
       barber->tools -= 4;
       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       BarberChair* bbchair6 = barber_chair(barber->shop,barber->chairPosition);
       bbchair6->toolsHolded -= 4;
       bci_set_syncBBChair(*bbchair6,barber->id);
     }

     if(barber->reqToDo == 1 or barber->reqToDo == 4) {
       bci_get_syncBBChair(barber_chair(barber->shop,barber->chairPosition),barber->id);
       release_barber_chair(barber_chair(barber->shop,barber->chairPosition), barber->id);
       BarberChair* bbchair7 = barber_chair(barber->shop,barber->chairPosition);
       bci_set_syncBBChair(*bbchair7,barber->id);
       barber->chairPosition = -1;
     }
     else {
       bci_get_syncWashbasin(washbasin(barber->shop,barber->basinPosition),barber->id);
       release_washbasin(washbasin(barber->shop,barber->basinPosition),barber->id);
       Washbasin* tmp_wsh2 = washbasin(barber->shop,barber->basinPosition);
       bci_set_syncWashbasin(*tmp_wsh2,barber->id);
       barber->basinPosition = -1;
     }

     bci_set_state(barber->id,PROCESS_DONE);

     log_barber(barber);
	 	 
     bci_did_request(barber->clientID);

     if(bci_get_request(barber->id) == 0) {
       bci_set_state(barber->id,ALL_PROCESSES_DONE);
     }
   }

   log_barber(barber);
}

static void release_client(Barber* barber)
{
   /** TODO:
    * 1: notify client that all the services are done
    **/

   require (barber != NULL, "barber argument required");

   client_done(barber->shop,barber->clientID);
   barber->clientID = 0;
   
   bci_unset_clientID(barber->id);

   log_barber(barber);
}

static void done(Barber* barber)
{
   /** TODO:
    * 1: set the client state to DONE
    **/
   require (barber != NULL, "barber argument required");

   bci_set_state(barber->id,ALL_PROCESSES_DONE);

   log_barber(barber);
}

static void process_haircut_request(Barber* barber)
{
   /** TODO:
    * ([incomplete] example code for task completion algorithm)
    **/
   require (barber != NULL, "barber argument required");
   require (barber->tools & SCISSOR_TOOL, "barber not holding a scissor");
   require (barber->tools & COMB_TOOL, "barber not holding a comb");

   int steps = random_int(5,20);
   int slice = (global->MAX_WORK_TIME_UNITS-global->MIN_WORK_TIME_UNITS+steps)/steps;
   int complete = 0;
   while(complete < 100)
   {
     spend(slice);
     complete += 100/steps;
     if (complete > 100) {
       complete = 100;
     }
     set_completion_barber_chair(barber_chair(barber->shop, barber->chairPosition), complete);
   }
}

static void process_hairwash_request(Barber* barber)
{
   require (barber != NULL, "barber argument required");

   int steps = random_int(5,20);
   int washes = (global->MAX_WORK_TIME_UNITS-global->MIN_WORK_TIME_UNITS+steps)/steps;
   int complete = 0;
   while(complete < 100)
   {
     spend(washes);
     complete += 100/steps;
     if (complete > 100) {
       complete = 100;
     }
     set_completion_washbasin(washbasin(barber->shop, barber->basinPosition), complete);
   }
}

static void process_shave_request(Barber* barber)
{
   require (barber != NULL, "barber argument required");
   require (barber->tools & RAZOR_TOOL, "barber not holding a razor");

   int steps = random_int(5,20);
   int slice = (global->MAX_WORK_TIME_UNITS-global->MIN_WORK_TIME_UNITS+steps)/steps;
   int complete = 0;
   while(complete < 100)
   {
     spend(slice);
     complete += 100/steps;
     if (complete > 100) {
       complete = 100;
     }
     set_completion_barber_chair(barber_chair(barber->shop, barber->chairPosition), complete);
   }
}

static char* to_string_barber(Barber* barber)
{
   require (barber != NULL, "barber argument required");

   if (barber->internal == NULL)
      barber->internal = (char*)mem_alloc(skel_length + 1);

   char tools[4];
   tools[0] = (barber->tools & SCISSOR_TOOL) ? 'S' : '-',
      tools[1] = (barber->tools & COMB_TOOL) ?    'C' : '-',
      tools[2] = (barber->tools & RAZOR_TOOL) ?   'R' : '-',
      tools[3] = '\0';

   char* pos = (char*)"-";
   if (barber->chairPosition >= 0)
      pos = int2nstr(barber->chairPosition+1, 1);
   else if (barber->basinPosition >= 0)
      pos = int2nstr(barber->basinPosition+1, 1);

   return gen_boxes(barber->internal, skel_length, skel,
         int2nstr(barber->id, 2),
         barber->clientID > 0 ? int2nstr(barber->clientID, 2) : "--",
         tools, stateText[barber->state], pos);
}

