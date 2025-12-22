#include <stdio.h>
#include <vector>
#include <cmath>
#include <algorithm> // Για το std::clamp (αν ειναι C++17) ή manually
#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"
#include "hls_math.h"

typedef float data_t;

// --- ΔΙΑΣΤΑΣΕΙΣ (Original) ---
const int ROWS = 16; 
const int COLUMNS = 1024;
const int TOTAL_ELEMENTS = ROWS * COLUMNS;

// --- ΔΙΑΣΤΑΣΕΙΣ (Padded για το HLS Input) ---
// Προσθέτουμε +2 σε κάθε διάσταση (Top/Bottom, Left/Right)
const int PAD_ROWS = ROWS + 2;      // 18
const int PAD_COLS = COLUMNS + 2;   // 1026
const int PAD_TOTAL_ELEMENTS = PAD_ROWS * PAD_COLS;

// Η έξοδος είναι το αρχικό μέγεθος (κρατάμε το Halo!)
const int OUT_ITERATIONS = TOTAL_ELEMENTS; 

// Prototype of the top-level function
// Προσοχή: Το A_in_mem πρέπει να δείχνει στο Padded Buffer
void architecture_top_level(data_t* A_in_mem, data_t* B_out_mem);

// --- GOLDEN MODEL (Με Replicate Padding Logic) ---
void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {

    printf("  [Golden] Starting golden computation (Full Image with Padding)...\n");
    B_golden_vec.clear();

    // Τώρα τρέχουμε σε ΟΛΗ την εικόνα (0 έως ROWS), όχι (1 έως ROWS-1)
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLUMNS; j++) {
            
            // 1. Εύρεση Συντεταγμένων Γειτόνων (Με "Clamping")
            // Αν βγούμε εκτός ορίων, μένουμε στο όριο (Replicate Padding)
            // Αυτό κάνει μαθηματικά το (Center - Neighbor) = 0 στα άκρα.
            
            int r_down  = (i == ROWS - 1)    ? i : i + 1; // Κάτω όριο
            int r_up    = (i == 0)           ? i : i - 1; // Πάνω όριο
            int c_right = (j == COLUMNS - 1) ? j : j + 1; // Δεξί όριο
            int c_left  = (j == 0)           ? j : j - 1; // Αριστερό όριο

            // 2. Ανάγνωση από τον 1D Vector (Row-Major)
            data_t a00  = A_vec[i * COLUMNS + j];       // Center
            data_t a10  = A_vec[r_down * COLUMNS + j];  // Down
            data_t a01  = A_vec[i * COLUMNS + c_right]; // Right
            data_t a0m1 = A_vec[i * COLUMNS + c_left];  // Left
            data_t am10 = A_vec[r_up * COLUMNS + j];    // Up

            // 3. Υπολογισμός (Όπως στο Kernel)
            data_t res_0 = a00 - a0m1;
            data_t res_1 = a00 - a01;
            data_t res_2 = a00 - am10;
            data_t res_3 = a00 - a10;

            data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                           (res_2 * res_2) + (res_3 * res_3);

            B_golden_vec.push_back(b_val);
        }
    }
    printf("  [Golden] Finished. Produced %zu outputs.\n", B_golden_vec.size());
}

int main() {
    printf("[TB] Starting Testbench...\n");

    // 1. Δέσμευση Μνήμης
    std::vector<data_t> RAM_in(TOTAL_ELEMENTS);         // Αρχική Εικόνα (16x1024)
    std::vector<data_t> RAM_padded_in(PAD_TOTAL_ELEMENTS); // Padded Εικόνα (18x1026) -> Input στο HLS
    std::vector<data_t> RAM_out(TOTAL_ELEMENTS);        // Έξοδος HLS (16x1024)
    std::vector<data_t> Golden_out;                     // Έξοδος Golden

    // 2. Αρχικοποίηση Δεδομένων (π.χ. Τιμή = Αριθμός Γραμμής)
    // Αυτό βοηθάει στο debug: Κάθετη διαφορά = 1, Οριζόντια = 0
    printf("[TB] Initializing input memory...\n");
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        RAM_in[i] = (data_t)(i / COLUMNS); // Όλη η γραμμή 0 έχει τιμή 0, η γραμμή 1 τιμή 1...
    }

    // 3. Δημιουργία Padded Image (SOFTWARE PADDING) [cite: 592]
    // Αντιγράφουμε τα δεδομένα στον μεγαλύτερο πίνακα 18x1026 με Replicate Logic
    printf("[TB] Generating Padded Input for HLS...\n");
    
    for (int r = 0; r < PAD_ROWS; r++) {     // 0..17
        for (int c = 0; c < PAD_COLS; c++) { // 0..1025
            
            // Μετατροπή συντεταγμένων Padded -> Original
            // Το (1,1) του Padded αντιστοιχεί στο (0,0) του Original.
            int orig_r = r - 1;
            int orig_c = c - 1;

            // CLAMPING (Replicate Border)
            if (orig_r < 0) orig_r = 0;
            if (orig_r >= ROWS) orig_r = ROWS - 1;
            
            if (orig_c < 0) orig_c = 0;
            if (orig_c >= COLUMNS) orig_c = COLUMNS - 1;

            // Αντιγραφή τιμής
            RAM_padded_in[r * PAD_COLS + c] = RAM_in[orig_r * COLUMNS + orig_c];
        }
    }

    // 4. Υπολογισμός Golden (πάνω στα Original Data)
    compute_golden(RAM_in, Golden_out);

    // 5. Εκτέλεση HLS Kernel
    // ΠΡΟΣΟΧΗ: Δίνουμε τον PADDED πίνακα ως είσοδο, αλλά τον ΚΑΝΟΝΙΚΟ ως έξοδο.
    printf("[TB] Calling 'architecture_top_level'...\n");
    
    architecture_top_level(RAM_padded_in.data(), RAM_out.data());
    
    printf("[TB] Execution finished.\n");

    // 6. Επαλήθευση
    printf("[TB] Verifying results...\n");
    int errors = 0;
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        data_t hls_val = RAM_out[i];
        data_t ref_val = Golden_out[i];

        if (std::abs(hls_val - ref_val) > 0.001f) {
            errors++;
            if (errors < 20) { // Τυπώνουμε τα πρώτα 20 λάθη
                int r = i / COLUMNS;
                int c = i % COLUMNS;
                printf("  [ERROR] @(%2d,%4d) HLS=%f Ref=%f\n", r, c, hls_val, ref_val);
            }
        }
    }

    if (errors == 0) {
        printf("\n--- TEST PASSED ---\n");
        printf("All %d pixels matches exactly.\n", TOTAL_ELEMENTS);
    } else {
        printf("\n--- TEST FAILED: %d mismatches found ---\n", errors);
    }

    return (errors == 0) ? 0 : 1;
}