#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define TOTAL_SLOTS 20
#define MAX_VEHICLE_NUMBER 20
#define PARKING_FILE "parking_records.txt"
#define TRANSACTION_FILE "transactions.txt"

#define TWO_WHEELER 1
#define FOUR_WHEELER 2
#define OTHER 3

#define TWO_WHEELER_RATE 10.0
#define FOUR_WHEELER_RATE 20.0
#define OTHER_RATE 30.0

struct Vehicle {
    char vehicleNumber[MAX_VEHICLE_NUMBER];
    int vehicleType;
    int slotNumber;
    time_t entryTime;
    int occupied;
};

struct Transaction {
    char vehicleNumber[MAX_VEHICLE_NUMBER];
    int vehicleType;
    int slotNumber;
    time_t entryTime;
    time_t exitTime;
    double durationHours;
    double amountPaid;
};

struct Vehicle parking[TOTAL_SLOTS];
int occupiedSlots = 0;

/* Function prototypes */
void initializeParking(void);
void displayMenu(void);
int readLine(const char *prompt, char buffer[], size_t size);
int readIntegerInRange(const char *prompt, int min, int max);
void normalizeVehicleNumber(char number[]);
int validateVehicleNumber(const char number[]);
int findVehicle(const char number[]);
int findFreeSlot(void);
void loadRecords(void);
int saveRecords(void);
int appendTransaction(const struct Transaction *transaction);
void vehicleEntry(void);
void vehicleExit(void);
void showAvailability(void);
void searchVehicle(void);
void displayVehicles(void);
void revenueReport(void);
const char *getVehicleTypeName(int type);
long long calculateBillableHours(double elapsedSeconds);
double calculateFee(int type, long long billableHours);
void printTime(time_t value);

/* Read a line safely. Returns 1=success, 0=EOF, -1=input too long. */
int readLine(const char *prompt, char buffer[], size_t size)
{
    size_t len;
    int ch;

    if (prompt != NULL) printf("%s", prompt);
    if (fgets(buffer, (int)size, stdin) == NULL) return 0;

    len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
        return 1;
    }

    if (!feof(stdin)) {
        while ((ch = getchar()) != '\n' && ch != EOF) {}
        buffer[0] = '\0';
        printf("Input is too long. Try again.\n");
        return -1;
    }
    return 1;
}

/* Accept only a whole number within the requested range. */
int readIntegerInRange(const char *prompt, int min, int max)
{
    char input[100], *end;
    long value;
    int status;

    while (1) {
        status = readLine(prompt, input, sizeof(input));
        if (status == 0) return min - 1;
        if (status == -1) continue;

        value = strtol(input, &end, 10);
        while (isspace((unsigned char)*end)) end++;

        if (input[0] == '\0' || end == input || *end != '\0') {
            printf("Enter a valid whole number.\n");
        } else if (value < min || value > max) {
            printf("Enter a number from %d to %d.\n", min, max);
        } else {
            return (int)value;
        }
    }
}

void initializeParking(void)
{
    int i;
    for (i = 0; i < TOTAL_SLOTS; i++) {
        memset(&parking[i], 0, sizeof(parking[i]));
        parking[i].slotNumber = i + 1;
    }
    occupiedSlots = 0;
}

void displayMenu(void)
{
    printf("\n========================================\n");
    printf("       SMART CAMPUS PARKING SYSTEM\n");
    printf("========================================\n");
    printf("1. Vehicle Entry\n");
    printf("2. Vehicle Exit\n");
    printf("3. View Parking Availability\n");
    printf("4. Search Vehicle\n");
    printf("5. Display Parked Vehicles\n");
    printf("6. Parking Revenue Report\n");
    printf("7. Exit\n");
    printf("----------------------------------------\n");
}

void normalizeVehicleNumber(char number[])
{
    int i;
    for (i = 0; number[i] != '\0'; i++)
        number[i] = (char)toupper((unsigned char)number[i]);
}

/* 1-19 characters; letters, digits and hyphens only. */
int validateVehicleNumber(const char number[])
{
    int i, hasAlphaNumeric = 0;
    size_t len = strlen(number);

    if (len == 0 || len >= MAX_VEHICLE_NUMBER) return 0;

    for (i = 0; number[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)number[i];
        if (isalnum(ch)) hasAlphaNumeric = 1;
        else if (number[i] != '-') return 0;
    }
    return hasAlphaNumeric;
}

/* Return array index of vehicle, or -1 if it is not parked. */
int findVehicle(const char number[])
{
    int i;
    for (i = 0; i < TOTAL_SLOTS; i++) {
        if (parking[i].occupied &&
            strcmp(parking[i].vehicleNumber, number) == 0)
            return i;
    }
    return -1;
}

/* Return the first free array index, or -1 if parking is full. */
int findFreeSlot(void)
{
    int i;
    for (i = 0; i < TOTAL_SLOTS; i++)
        if (!parking[i].occupied) return i;
    return -1;
}

/*
 * Active record format:
 * registration|type|slot|entryTimestamp
 */
void loadRecords(void)
{
    FILE *fp;
    char line[200], number[MAX_VEHICLE_NUMBER];
    int type, slot, fields, index;
    long long timestamp;

    fp = fopen(PARKING_FILE, "r");
    if (fp == NULL) {
        printf("No old parking file found. Starting empty.\n");
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        fields = sscanf(line, "%19[^|]|%d|%d|%lld",
                        number, &type, &slot, &timestamp);
        if (fields != 4) {
            printf("Skipping invalid parking record.\n");
            continue;
        }

        normalizeVehicleNumber(number);
        if (!validateVehicleNumber(number) ||
            type < TWO_WHEELER || type > OTHER ||
            slot < 1 || slot > TOTAL_SLOTS) {
            printf("Skipping out-of-range parking record.\n");
            continue;
        }

        index = slot - 1;
        if (parking[index].occupied || findVehicle(number) != -1) {
            printf("Skipping duplicate or conflicting parking record.\n");
            continue;
        }

        strcpy(parking[index].vehicleNumber, number);
        parking[index].vehicleType = type;
        parking[index].slotNumber = slot;
        parking[index].entryTime = (time_t)timestamp;
        parking[index].occupied = 1;
        occupiedSlots++;
    }
    fclose(fp);
}

/* Save the current active vehicles. */
int saveRecords(void)
{
    FILE *fp;
    int i, ok = 1;

    fp = fopen(PARKING_FILE, "w");
    if (fp == NULL) {
        printf("ERROR: Cannot write %s.\n", PARKING_FILE);
        return 0;
    }

    for (i = 0; i < TOTAL_SLOTS; i++) {
        if (parking[i].occupied &&
            fprintf(fp, "%s|%d|%d|%lld\n",
                    parking[i].vehicleNumber,
                    parking[i].vehicleType,
                    parking[i].slotNumber,
                    (long long)parking[i].entryTime) < 0) {
            ok = 0;
            break;
        }
    }

    if (fclose(fp) != 0) ok = 0;
    if (!ok) printf("ERROR: Parking records may not have been fully saved.\n");
    return ok;
}

/* Append one completed parking transaction. */
int appendTransaction(const struct Transaction *t)
{
    FILE *fp;
    int ok;

    fp = fopen(TRANSACTION_FILE, "a");
    if (fp == NULL) {
        printf("ERROR: Cannot open %s.\n", TRANSACTION_FILE);
        return 0;
    }

    ok = fprintf(fp, "%s|%d|%d|%lld|%lld|%.4f|%.2f\n",
                 t->vehicleNumber, t->vehicleType, t->slotNumber,
                 (long long)t->entryTime, (long long)t->exitTime,
                 t->durationHours, t->amountPaid) >= 0;
    if (fclose(fp) != 0) ok = 0;
    if (!ok) printf("ERROR: Transaction could not be saved.\n");
    return ok;
}

const char *getVehicleTypeName(int type)
{
    switch (type) {
        case TWO_WHEELER: return "Two-wheeler";
        case FOUR_WHEELER: return "Four-wheeler";
        case OTHER: return "Other";
        default: return "Unknown";
    }
}

/* Round every started hour upward; minimum one billable hour. */
long long calculateBillableHours(double seconds)
{
    long long hours;
    if (seconds <= 0) return 1;
    hours = (long long)((seconds + 3599.0) / 3600.0);
    return hours < 1 ? 1 : hours;
}

double calculateFee(int type, long long hours)
{
    double rate;
    switch (type) {
        case TWO_WHEELER: rate = TWO_WHEELER_RATE; break;
        case FOUR_WHEELER: rate = FOUR_WHEELER_RATE; break;
        case OTHER: rate = OTHER_RATE; break;
        default: return 0.0;
    }
    return rate * hours;
}

void printTime(time_t value)
{
    struct tm *info = localtime(&value);
    char buffer[80];

    if (info == NULL ||
        strftime(buffer, sizeof(buffer), "%d-%m-%Y %I:%M:%S %p", info) == 0)
        printf("Unknown time");
    else
        printf("%s", buffer);
}

/* Add a vehicle to the lowest-numbered free slot. */
void vehicleEntry(void)
{
    char number[MAX_VEHICLE_NUMBER];
    int status, type, index;
    time_t now;

    printf("\n---------- VEHICLE ENTRY ----------\n");
    status = readLine("Registration number: ", number, sizeof(number));
    if (status != 1) return;

    normalizeVehicleNumber(number);
    if (!validateVehicleNumber(number)) {
        printf("Invalid number. Use 1-19 letters, digits or hyphens.\n");
        return;
    }
    if (findVehicle(number) != -1) {
        printf("This vehicle is already parked. Duplicate rejected.\n");
        return;
    }

    index = findFreeSlot();
    if (index == -1) {
        printf("Parking area is FULL.\n");
        return;
    }

    printf("1. Two-wheeler (Rs. %.2f/hour)\n", TWO_WHEELER_RATE);
    printf("2. Four-wheeler (Rs. %.2f/hour)\n", FOUR_WHEELER_RATE);
    printf("3. Other (Rs. %.2f/hour)\n", OTHER_RATE);
    type = readIntegerInRange("Vehicle category (1-3): ", 1, 3);
    if (type < 1 || type > 3) return;

    now = time(NULL);
    if (now == (time_t)-1) {
        printf("Could not get system time. Entry cancelled.\n");
        return;
    }

    strcpy(parking[index].vehicleNumber, number);
    parking[index].vehicleType = type;
    parking[index].slotNumber = index + 1;
    parking[index].entryTime = now;
    parking[index].occupied = 1;
    occupiedSlots++;

    if (!saveRecords()) {
        memset(&parking[index], 0, sizeof(parking[index]));
        parking[index].slotNumber = index + 1;
        occupiedSlots--;
        printf("Entry was cancelled because saving failed.\n");
        return;
    }

    printf("Entry successful. Slot number: %d\n", index + 1);
    printf("Category: %s\n", getVehicleTypeName(type));
    printf("Entry time: ");
    printTime(now);
    printf("\nAvailable slots: %d\n", TOTAL_SLOTS - occupiedSlots);
}

/* Calculate bill, remove the active vehicle and save its transaction. */
void vehicleExit(void)
{
    char number[MAX_VEHICLE_NUMBER];
    int status, index;
    struct Vehicle oldVehicle;
    struct Transaction t;
    time_t now;
    double seconds;
    long long hours;

    printf("\n---------- VEHICLE EXIT ----------\n");
    status = readLine("Registration number: ", number, sizeof(number));
    if (status != 1) return;

    normalizeVehicleNumber(number);
    if (!validateVehicleNumber(number)) {
        printf("Invalid registration number.\n");
        return;
    }

    index = findVehicle(number);
    if (index == -1) {
        printf("Vehicle is not currently parked.\n");
        return;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        printf("Could not get system time. Exit cancelled.\n");
        return;
    }

    seconds = difftime(now, parking[index].entryTime);
    if (seconds < 0) {
        printf("System time is earlier than entry time. Exit cancelled.\n");
        return;
    }

    hours = calculateBillableHours(seconds);
    strcpy(t.vehicleNumber, parking[index].vehicleNumber);
    t.vehicleType = parking[index].vehicleType;
    t.slotNumber = parking[index].slotNumber;
    t.entryTime = parking[index].entryTime;
    t.exitTime = now;
    t.durationHours = seconds / 3600.0;
    t.amountPaid = calculateFee(t.vehicleType, hours);

    printf("\n--------------- PARKING BILL ---------------\n");
    printf("Registration : %s\n", t.vehicleNumber);
    printf("Category     : %s\n", getVehicleTypeName(t.vehicleType));
    printf("Slot         : %d\n", t.slotNumber);
    printf("Entry        : "); printTime(t.entryTime);
    printf("\nExit         : "); printTime(t.exitTime);
    printf("\nDuration     : %.2f hour(s)\n", t.durationHours);
    printf("Billable hrs : %lld\n", hours);
    printf("Amount       : Rs. %.2f\n", t.amountPaid);
    printf("--------------------------------------------\n");

    /*
     * Save the active list without this vehicle first.
     * If the transaction append fails, restore the vehicle.
     */
    oldVehicle = parking[index];
    memset(&parking[index], 0, sizeof(parking[index]));
    parking[index].slotNumber = index + 1;
    occupiedSlots--;

    if (!saveRecords()) {
        parking[index] = oldVehicle;
        occupiedSlots++;
        printf("Exit cancelled. Vehicle remains active in memory.\n");
        return;
    }

    if (!appendTransaction(&t)) {
        parking[index] = oldVehicle;
        occupiedSlots++;
        if (!saveRecords())
            printf("CRITICAL: Could not restore %s. Check the parking file.\n",
                   PARKING_FILE);
        else
            printf("Vehicle remains parked because transaction saving failed.\n");
        return;
    }

    printf("Exit completed. Slot %d is now available.\n", index + 1);
}

void showAvailability(void)
{
    int i;
    printf("\n---------- PARKING AVAILABILITY ----------\n");
    for (i = 0; i < TOTAL_SLOTS; i++) {
        if (parking[i].occupied)
            printf("Slot %2d: OCCUPIED - %s\n", i + 1,
                   parking[i].vehicleNumber);
        else
            printf("Slot %2d: AVAILABLE\n", i + 1);
    }
    printf("Occupied: %d | Available: %d | Total: %d\n",
           occupiedSlots, TOTAL_SLOTS - occupiedSlots, TOTAL_SLOTS);
}

void searchVehicle(void)
{
    char number[MAX_VEHICLE_NUMBER];
    int status, index;

    status = readLine("Registration number to search: ",
                      number, sizeof(number));
    if (status != 1) return;

    normalizeVehicleNumber(number);
    if (!validateVehicleNumber(number)) {
        printf("Invalid registration number.\n");
        return;
    }

    index = findVehicle(number);
    if (index == -1) {
        printf("Vehicle is not currently parked.\n");
        return;
    }

    printf("Vehicle found: %s\n", parking[index].vehicleNumber);
    printf("Category: %s\n", getVehicleTypeName(parking[index].vehicleType));
    printf("Slot: %d\n", parking[index].slotNumber);
    printf("Entry time: ");
    printTime(parking[index].entryTime);
    printf("\n");
}

void displayVehicles(void)
{
    int i, found = 0;

    printf("\n---------- CURRENTLY PARKED VEHICLES ----------\n");
    printf("%-20s %-16s %-8s %s\n",
           "Registration", "Category", "Slot", "Entry time");

    for (i = 0; i < TOTAL_SLOTS; i++) {
        if (parking[i].occupied) {
            found = 1;
            printf("%-20s %-16s %-8d ",
                   parking[i].vehicleNumber,
                   getVehicleTypeName(parking[i].vehicleType),
                   parking[i].slotNumber);
            printTime(parking[i].entryTime);
            printf("\n");
        }
    }
    if (!found) printf("No vehicles are parked.\n");
}

/* Read the completed transaction file and total the revenue. */
void revenueReport(void)
{
    FILE *fp;
    char line[300], number[MAX_VEHICLE_NUMBER];
    int type, slot, fields, count = 0;
    long long entry, exit;
    double duration, amount;
    double total = 0, two = 0, four = 0, other = 0;

    fp = fopen(TRANSACTION_FILE, "r");
    if (fp == NULL) {
        printf("No completed transactions yet.\n");
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        fields = sscanf(line, "%19[^|]|%d|%d|%lld|%lld|%lf|%lf",
                        number, &type, &slot, &entry, &exit,
                        &duration, &amount);
        if (fields != 7) {
            printf("Skipping an invalid transaction record.\n");
            continue;
        }

        count++;
        total += amount;
        if (type == TWO_WHEELER) two += amount;
        else if (type == FOUR_WHEELER) four += amount;
        else if (type == OTHER) other += amount;
    }
    fclose(fp);

    printf("\n---------- REVENUE REPORT ----------\n");
    printf("Completed transactions : %d\n", count);
    printf("Two-wheeler revenue    : Rs. %.2f\n", two);
    printf("Four-wheeler revenue   : Rs. %.2f\n", four);
    printf("Other vehicle revenue  : Rs. %.2f\n", other);
    printf("TOTAL REVENUE          : Rs. %.2f\n", total);
}

int main(void)
{
    int choice;

    initializeParking();
    loadRecords();

    printf("\nWelcome to Smart Campus Parking System!\n");
    printf("Loaded %d active vehicle(s).\n", occupiedSlots);

    while (1) {
        displayMenu();
        choice = readIntegerInRange("Enter choice (1-7): ", 1, 7);

        if (choice < 1 || choice > 7) {
            printf("Input ended. Closing program.\n");
            break;
        }

        switch (choice) {
            case 1: vehicleEntry(); break;
            case 2: vehicleExit(); break;
            case 3: showAvailability(); break;
            case 4: searchVehicle(); break;
            case 5: displayVehicles(); break;
            case 6: revenueReport(); break;
            case 7:
                printf("Thank you for using the system.\n");
                return 0;
        }
    }
    return 0;
}
