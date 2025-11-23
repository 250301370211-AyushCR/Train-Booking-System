#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TRAINS 20
#define MAX_TICKETS 1000
#define MAX_WAITING 1000
#define NAME_LEN 50
#define TRAIN_STATION_LEN 30

typedef struct {
    int id;
    char name[NAME_LEN];
    char from[TRAIN_STATION_LEN];
    char to[TRAIN_STATION_LEN];
    int totalSeats;
    int bookedSeats;
} Train;

typedef struct {
    int pnr;
    int trainId;
    char passengerName[NAME_LEN];
    int age;
    int seatNo;      // 0 if not confirmed
    char status[10]; // "CNF", "WL", "CNL"
} Ticket;

typedef struct WaitingNode {
    int pnr;
    int trainId;
    char passengerName[NAME_LEN];
    int age;
    struct WaitingNode *next;
} WaitingNode;

typedef struct {
    int pnr;
    int trainId;
    char passengerName[NAME_LEN];
    int age;
} WaitingEntry;

/* Global arrays & counts */
Train trains[MAX_TRAINS];
int trainCount = 0;

Ticket tickets[MAX_TICKETS];
int ticketCount = 0;

WaitingNode *waitingFront = NULL;
WaitingNode *waitingRear = NULL;
int waitingCount = 0;

int lastPNR = 1000;

/* Admin */
int isAdminLoggedIn = 0;
const char ADMIN_PASSWORD[] = "admin123";

/* ---------- Utility Functions ---------- */

void flushInput() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void pressEnter() {
    printf("\nPress Enter to continue...");
    flushInput();
}

int generatePNR() {
    lastPNR++;
    return lastPNR;
}

Train* findTrainById(int id) {
    for (int i = 0; i < trainCount; i++) {
        if (trains[i].id == id) return &trains[i];
    }
    return NULL;
}

Ticket* findTicketByPNR(int pnr) {
    for (int i = 0; i < ticketCount; i++) {
        if (tickets[i].pnr == pnr) return &tickets[i];
    }
    return NULL;
}

/* ---------- Waiting List (in-memory linked list) ---------- */

void enqueueWaitingNode(int pnr, int trainId, const char *name, int age) {
    WaitingNode *node = (WaitingNode*)malloc(sizeof(WaitingNode));
    if (!node) {
        printf("Memory full. Cannot add to waiting list.\n");
        return;
    }
    node->pnr = pnr;
    node->trainId = trainId;
    strncpy(node->passengerName, name, NAME_LEN);
    node->passengerName[NAME_LEN-1] = '\0';
    node->age = age;
    node->next = NULL;

    if (waitingRear == NULL) {
        waitingFront = waitingRear = node;
    } else {
        waitingRear->next = node;
        waitingRear = node;
    }
    waitingCount++;
}

WaitingNode* dequeueWaitingForTrain(int trainId) {
    WaitingNode *prev = NULL;
    WaitingNode *curr = waitingFront;

    while (curr) {
        if (curr->trainId == trainId) {
            if (prev == NULL) {
                waitingFront = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (curr == waitingRear) {
                waitingRear = prev;
            }
            curr->next = NULL;
            waitingCount--;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

int removeWaitingByPNR(int pnr) {
    WaitingNode *prev = NULL;
    WaitingNode *curr = waitingFront;

    while (curr) {
        if (curr->pnr == pnr) {
            if (prev == NULL)
                waitingFront = curr->next;
            else
                prev->next = curr->next;
            if (curr == waitingRear)
                waitingRear = prev;
            free(curr);
            waitingCount--;
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

/* ---------- File Handling ---------- */

void savePNR() {
    FILE *fp = fopen("pnr.dat", "wb");
    if (!fp) return;
    fwrite(&lastPNR, sizeof(int), 1, fp);
    fclose(fp);
}

void loadPNR() {
    FILE *fp = fopen("pnr.dat", "rb");
    if (!fp) return;
    if (fread(&lastPNR, sizeof(int), 1, fp) != 1) {
        lastPNR = 1000;
    }
    fclose(fp);
}

void loadTrains() {
    FILE *fp = fopen("trains.dat", "rb");
    if (!fp) return; // first time
    if (fread(&trainCount, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return;
    }
    if (trainCount > 0 && trainCount <= MAX_TRAINS) {
        size_t read = fread(trains, sizeof(Train), (size_t)trainCount, fp);
        if (read != (size_t)trainCount) {
            /* corrupted file or partial read */
            trainCount = 0;
        }
    } else {
        trainCount = 0;
    }
    fclose(fp);
}

void saveTrains() {
    FILE *fp = fopen("trains.dat", "wb");
    if (!fp) {
        printf("Error saving trains file.\n");
        return;
    }
    fwrite(&trainCount, sizeof(int), 1, fp);
    if (trainCount > 0)
        fwrite(trains, sizeof(Train), (size_t)trainCount, fp);
    fclose(fp);
}

void loadTickets() {
    FILE *fp = fopen("tickets.dat", "rb");
    if (!fp) return;
    if (fread(&ticketCount, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return;
    }
    if (ticketCount > 0 && ticketCount <= MAX_TICKETS) {
        size_t read = fread(tickets, sizeof(Ticket), (size_t)ticketCount, fp);
        if (read != (size_t)ticketCount) {
            ticketCount = 0;
        }
    } else {
        ticketCount = 0;
    }
    fclose(fp);
}

void saveTickets() {
    FILE *fp = fopen("tickets.dat", "wb");
    if (!fp) {
        printf("Error saving tickets file.\n");
        return;
    }
    fwrite(&ticketCount, sizeof(int), 1, fp);
    if (ticketCount > 0)
        fwrite(tickets, sizeof(Ticket), (size_t)ticketCount, fp);
    fclose(fp);
}

void saveWaiting() {
    FILE *fp = fopen("waiting.dat", "wb");
    if (!fp) {
        printf("Could not save waiting list.\n");
        return;
    }
    /* Convert linked list to array for persistence */
    WaitingEntry entries[MAX_WAITING];
    int idx = 0;
    WaitingNode *curr = waitingFront;
    while (curr && idx < MAX_WAITING) {
        entries[idx].pnr = curr->pnr;
        entries[idx].trainId = curr->trainId;
        strncpy(entries[idx].passengerName, curr->passengerName, NAME_LEN);
        entries[idx].passengerName[NAME_LEN-1] = '\0';
        entries[idx].age = curr->age;
        idx++;
        curr = curr->next;
    }
    fwrite(&idx, sizeof(int), 1, fp);
    if (idx > 0)
        fwrite(entries, sizeof(WaitingEntry), (size_t)idx, fp);
    fclose(fp);
}

void loadWaiting() {
    FILE *fp = fopen("waiting.dat", "rb");
    if (!fp) return;
    int cnt = 0;
    if (fread(&cnt, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return;
    }
    if (cnt < 0 || cnt > MAX_WAITING) cnt = 0;
    WaitingEntry entries[MAX_WAITING];
    if (cnt > 0) {
        if (fread(entries, sizeof(WaitingEntry), (size_t)cnt, fp) != (size_t)cnt) {
            cnt = 0;
        }
    }
    /* rebuild linked list */
    for (int i = 0; i < cnt; i++) {
        enqueueWaitingNode(entries[i].pnr, entries[i].trainId, entries[i].passengerName, entries[i].age);
    }
    fclose(fp);
}

/* ---------- Core Features ---------- */

void seedSampleTrains() {
    if (trainCount > 0) return; /* already loaded or created */

    trains[0].id = 101;
    strcpy(trains[0].name, "Express A");
    strcpy(trains[0].from, "CITY1");
    strcpy(trains[0].to, "CITY2");
    trains[0].totalSeats = 5;
    trains[0].bookedSeats = 0;

    trains[1].id = 102;
    strcpy(trains[1].name, "Express B");
    strcpy(trains[1].from, "CITY2");
    strcpy(trains[1].to, "CITY3");
    trains[1].totalSeats = 5;
    trains[1].bookedSeats = 0;

    trainCount = 2;
    saveTrains();
}

void addTrain() {
    if (!isAdminLoggedIn) {
        printf("Admin privileges required to add a train.\n");
        return;
    }
    if (trainCount >= MAX_TRAINS) {
        printf("Cannot add more trains.\n");
        return;
    }
    Train t;
    printf("Enter train id: ");
    if (scanf("%d", &t.id) != 1) {
        printf("Invalid input.\n");
        flushInput();
        return;
    }
    flushInput();

    printf("Enter train name: ");
    fgets(t.name, NAME_LEN, stdin);
    t.name[strcspn(t.name, "\n")] = '\0';

    printf("From station: ");
    fgets(t.from, sizeof(t.from), stdin);
    t.from[strcspn(t.from, "\n")] = '\0';

    printf("To station: ");
    fgets(t.to, sizeof(t.to), stdin);
    t.to[strcspn(t.to, "\n")] = '\0';

    printf("Total seats: ");
    if (scanf("%d", &t.totalSeats) != 1) {
        printf("Invalid seats input.\n");
        flushInput();
        return;
    }
    t.bookedSeats = 0;
    flushInput();

    trains[trainCount++] = t;
    saveTrains();
    printf("Train added successfully!\n");
}

void listTrains() {
    printf("\n---- Available Trains ----\n");
    if (trainCount == 0) {
        printf("No trains available.\n");
        return;
    }
    for (int i = 0; i < trainCount; i++) {
        printf("ID: %d | %s | %s -> %s | Seats: %d | Booked: %d\n",
               trains[i].id, trains[i].name, trains[i].from, trains[i].to,
               trains[i].totalSeats, trains[i].bookedSeats);
    }
}

void bookTicket() {
    int trainId;
    char name[NAME_LEN];
    int age;

    if (trainCount == 0) {
        printf("No trains available to book.\n");
        return;
    }

    listTrains();
    printf("\nEnter Train ID to book: ");
    if (scanf("%d", &trainId) != 1) {
        printf("Invalid train id.\n");
        flushInput();
        return;
    }
    flushInput();

    Train *t = findTrainById(trainId);
    if (!t) {
        printf("Invalid train id.\n");
        return;
    }

    printf("Enter passenger name: ");
    fgets(name, NAME_LEN, stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Enter age: ");
    if (scanf("%d", &age) != 1) {
        printf("Invalid age input.\n");
        flushInput();
        return;
    }
    flushInput();

    if (ticketCount >= MAX_TICKETS) {
        printf("Ticket system full; cannot book more.\n");
        return;
    }

    Ticket ticket;
    ticket.pnr = generatePNR();
    ticket.trainId = trainId;
    strncpy(ticket.passengerName, name, NAME_LEN);
    ticket.passengerName[NAME_LEN-1] = '\0';
    ticket.age = age;

    if (t->bookedSeats < t->totalSeats) {
        t->bookedSeats++;
        ticket.seatNo = t->bookedSeats;
        strcpy(ticket.status, "CNF");
        printf("\nTicket Confirmed! Seat No: %d\n", ticket.seatNo);
    } else {
        ticket.seatNo = 0;
        strcpy(ticket.status, "WL");
        if (waitingCount >= MAX_WAITING) {
            printf("Waiting list full; cannot add.\n");
            return;
        }
        enqueueWaitingNode(ticket.pnr, trainId, name, age);
        printf("\nNo seats available. Added to Waiting List.\n");
    }

    tickets[ticketCount++] = ticket;
    saveTrains();
    saveTickets();
    savePNR();
    saveWaiting();

    printf("Your PNR: %d\n", ticket.pnr);
}

void showTicket() {
    int pnr;
    printf("Enter PNR: ");
    if (scanf("%d", &pnr) != 1) {
        printf("Invalid PNR.\n");
        flushInput();
        return;
    }
    flushInput();

    Ticket *t = findTicketByPNR(pnr);
    if (!t) {
        printf("Ticket not found.\n");
        return;
    }

    Train *tr = findTrainById(t->trainId);
    printf("\n---- Ticket Details ----\n");
    printf("PNR: %d\n", t->pnr);
    if (tr)
        printf("Train: %d - %s (%s -> %s)\n",
               tr->id, tr->name, tr->from, tr->to);
    else
        printf("Train ID: %d (details unavailable)\n", t->trainId);
    printf("Passenger: %s\nAge: %d\n", t->passengerName, t->age);
    printf("Status: %s\n", t->status);
    if (strcmp(t->status, "CNF") == 0)
        printf("Seat No: %d\n", t->seatNo);
}

void cancelTicket() {
    int pnr;
    printf("Enter PNR to cancel: ");
    if (scanf("%d", &pnr) != 1) {
        printf("Invalid PNR.\n");
        flushInput();
        return;
    }
    flushInput();

    Ticket *t = findTicketByPNR(pnr);
    if (!t) {
        printf("Ticket not found.\n");
        return;
    }

    Train *tr = findTrainById(t->trainId);
    if (!tr) {
        printf("Train not found.\n");
        return;
    }

    if (strcmp(t->status, "CNF") == 0) {
        int freedSeat = t->seatNo;
        printf("Confirmed ticket cancelled. Freed Seat No: %d\n", freedSeat);
        t->seatNo = 0;
        strcpy(t->status, "CNL");
        if (tr->bookedSeats > 0) tr->bookedSeats--;

        /* Upgrade first waiting passenger for this train */
        WaitingNode *w = dequeueWaitingForTrain(tr->id);
        if (w) {
            Ticket *wt = findTicketByPNR(w->pnr);
            if (wt && strcmp(wt->status, "WL") == 0) {
                wt->seatNo = freedSeat;
                strcpy(wt->status, "CNF");
                tr->bookedSeats++;
                printf("Waiting passenger %s upgraded to Confirmed (Seat %d).\n",
                       wt->passengerName, wt->seatNo);
            }
            free(w);
        }
    } else if (strcmp(t->status, "WL") == 0) {
        /* Remove from waiting list properly */
        if (removeWaitingByPNR(t->pnr)) {
            strcpy(t->status, "CNL");
            printf("Waiting list ticket cancelled and removed from queue.\n");
        } else {
            strcpy(t->status, "CNL");
            printf("Waiting list ticket cancelled (entry not found in queue).\n");
        }
    } else {
        printf("Ticket already cancelled.\n");
    }

    saveTrains();
    saveTickets();
    saveWaiting();
    savePNR();
}

void showWaitingList() {
    printf("\n---- Waiting List ----\n");
    WaitingNode *curr = waitingFront;
    int i = 1;
    while (curr) {
        printf("%d) PNR: %d | Train: %d | %s | Age: %d\n",
               i++, curr->pnr, curr->trainId,
               curr->passengerName, curr->age);
        curr = curr->next;
    }
    if (i == 1) printf("No passengers in waiting list.\n");
}

void viewAllTickets() {
    if (!isAdminLoggedIn) {
        printf("Admin privileges required.\n");
        return;
    }
    printf("\n---- All Tickets (%d) ----\n", ticketCount);
    for (int i = 0; i < ticketCount; i++) {
        Ticket *t = &tickets[i];
        Train *tr = findTrainById(t->trainId);
        if (tr) {
            printf("PNR: %d | Train: %d - %s | Name: %s | Age: %d | Status: %s",
                   t->pnr, t->trainId, tr->name, t->passengerName, t->age, t->status);
        } else {
            printf("PNR: %d | Train: %d | Name: %s | Age: %d | Status: %s",
                   t->pnr, t->trainId, t->passengerName, t->age, t->status);
        }
        if (strcmp(t->status, "CNF") == 0)
            printf(" | Seat: %d", t->seatNo);
        printf("\n");
    }
    if (ticketCount == 0) printf("No tickets booked yet.\n");
}

/* ---------- Authentication ---------- */

void adminLogin() {
    char password[100];
    printf("Enter admin password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = '\0';
    if (strcmp(password, ADMIN_PASSWORD) == 0) {
        isAdminLoggedIn = 1;
        printf("Admin login successful.\n");
    } else {
        isAdminLoggedIn = 0;
        printf("Incorrect password.\n");
    }
}

void adminLogout() {
    isAdminLoggedIn = 0;
    printf("Admin logged out.\n");
}

/* ---------- Main Menu ---------- */

void menu() {
    int choice;
    while (1) {
        printf("\n===== Railway Reservation System =====\n");
        printf("1. List Trains\n");
        if (isAdminLoggedIn) {
            printf("2. Add Train (Admin)\n");
            printf("3. Book Ticket\n");
            printf("4. Show Ticket\n");
            printf("5. Cancel Ticket\n");
            printf("6. Show Waiting List\n");
            printf("7. View All Tickets (Admin)\n");
            printf("8. Admin Logout\n");
            printf("9. Exit\n");
        } else {
            printf("2. Book Ticket\n");
            printf("3. Show Ticket\n");
            printf("4. Cancel Ticket\n");
            printf("5. Show Waiting List\n");
            printf("6. Admin Login\n");
            printf("7. Exit\n");
        }

        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1) {
            printf("Invalid choice. Try again.\n");
            flushInput();
            continue;
        }
        flushInput();

        if (isAdminLoggedIn) {
            switch (choice) {
                case 1: listTrains(); pressEnter(); break;
                case 2: addTrain(); pressEnter(); break;
                case 3: bookTicket(); pressEnter(); break;
                case 4: showTicket(); pressEnter(); break;
                case 5: cancelTicket(); pressEnter(); break;
                case 6: showWaitingList(); pressEnter(); break;
                case 7: viewAllTickets(); pressEnter(); break;
                case 8: adminLogout(); pressEnter(); break;
                case 9: saveTrains(); saveTickets(); saveWaiting(); savePNR(); printf("Exiting...\n"); return;
                default: printf("Invalid choice. Try again.\n");
            }
        } else {
            switch (choice) {
                case 1: listTrains(); pressEnter(); break;
                case 2: bookTicket(); pressEnter(); break;
                case 3: showTicket(); pressEnter(); break;
                case 4: cancelTicket(); pressEnter(); break;
                case 5: showWaitingList(); pressEnter(); break;
                case 6: adminLogin(); pressEnter(); break;
                case 7: saveTrains(); saveTickets(); saveWaiting(); savePNR(); printf("Exiting...\n"); return;
                default: printf("Invalid choice. Try again.\n");
            }
        }
    }
}

int main() {
    loadPNR();
    loadTrains();
    loadTickets();
    loadWaiting();
    seedSampleTrains();   /* adds 2 default trains if file is empty */
    menu();

    /* cleanup waiting nodes */
    WaitingNode *curr = waitingFront;
    while (curr) {
        WaitingNode *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    return 0;
}