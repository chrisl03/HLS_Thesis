#include <iostream>
#include <cmath>
#include <cstdlib>
#include "hls_stream.h"

typedef float data_t;

const int ORIG_ROWS = 16;
const int ORIG_COLS = 1024;
const int ORIG_TOTAL = ORIG_ROWS * ORIG_COLS;

// --- Καθυστερήσεις SODA (για πλάτος 1024) ---
const int FIFO_0_DEPTH = 1023; // Από Down σε Right
const int FIFO_1_DEPTH = 1;    // Από Right σε Center
const int FIFO_2_DEPTH = 1;    // Από Center σε Left
const int FIFO_3_DEPTH = 1023; // Από Left σε Up

void architecture_top_level(hls::stream<data_t>& A_in,
                          hls::stream<data_t>& B_out);

// Δήλωση του top-level module (αν το tb είναι σε ξεχωριστό αρχείο, αλλιώς αγνόησέ το)
// void architecture_top_level(hls::stream<data_t> &A_in, hls::stream<data_t> &B_out);

// --------------------------------------------------------
// 1. Το Golden Reference (Software Implementation)
// --------------------------------------------------------
void software_stencil(data_t in[ORIG_ROWS][ORIG_COLS], data_t out[ORIG_ROWS][ORIG_COLS]) {
    for (int i = 0; i < ORIG_ROWS; i++) {
        for (int j = 0; j < ORIG_COLS; j++) {
            
            // Υπολογίζουμε μόνο τα εσωτερικά πίξελ (εκεί που το παράθυρο δεν βγαίνει εκτός ορίων)
            if (i > 0 && i < ORIG_ROWS - 1 && j > 0 && j < ORIG_COLS - 1) {
                data_t a00  = in[i][j];     // Center
                data_t a10  = in[i+1][j];   // Down
                data_t a01  = in[i][j+1];   // Right
                data_t a0m1 = in[i][j-1];   // Left
                data_t am10 = in[i-1][j];   // Up

                data_t res_0 = a00 - a0m1;
                data_t res_1 = a00 - a01;
                data_t res_2 = a00 - am10;
                data_t res_3 = a00 - a10;

                out[i][j] = (res_0 * res_0) + (res_1 * res_1) +
                            (res_2 * res_2) + (res_3 * res_3);
            } else {
                out[i][j] = 0.0f; // Στα σύνορα βάζουμε 0 στο software
            }
        }
    }
}

// --------------------------------------------------------
// 2. Η συνάρτηση Main (Το Testbench)
// --------------------------------------------------------
int main() {
    // Δέσμευση μνήμης για τους πίνακες ελέγχου
    data_t A_in_array[ORIG_ROWS][ORIG_COLS];
    data_t B_hw_array[ORIG_ROWS][ORIG_COLS];
    data_t B_sw_array[ORIG_ROWS][ORIG_COLS];

    hls::stream<data_t> A_stream("A_stream");
    hls::stream<data_t> B_stream("B_stream");

    // ΒΗΜΑ 1: Δημιουργία τυχαίων δεδομένων (Test Data)
    std::cout << "Generating input data..." << std::endl;
    for (int i = 0; i < ORIG_ROWS; i++) {
        for (int j = 0; j < ORIG_COLS; j++) {
            // Βάζουμε τυχαίους δεκαδικούς από το 0.0 έως το 9.9
            A_in_array[i][j] = (data_t)(rand() % 100) / 10.0f; 
            
            // Παράλληλα, τα "ταΐζουμε" στο stream του hardware
            A_stream.write(A_in_array[i][j]); 
        }
    }

    // ΒΗΜΑ 2: Εκτέλεση του Software Golden Model
    std::cout << "Running Software Stencil..." << std::endl;
    software_stencil(A_in_array, B_sw_array);

    // ΒΗΜΑ 3: Εκτέλεση του Hardware (HLS Top Level)
    std::cout << "Running Hardware (HLS) core..." << std::endl;
    architecture_top_level(A_stream, B_stream);

    // ΒΗΜΑ 4: Ανάγνωση των αποτελεσμάτων από το Hardware stream
    for (int i = 0; i < ORIG_ROWS; i++) {
        for (int j = 0; j < ORIG_COLS; j++) {
            B_hw_array[i][j] = B_stream.read();
        }
    }

    // ΒΗΜΑ 5: Σύγκριση HW με SW (ΜΟΝΟ στα εσωτερικά πίξελ)
    std::cout << "Comparing results..." << std::endl;
    int errors = 0;
    
    // Προσέξτε τα όρια των loops! Αρχίζουν από 1 και τελειώνουν στο N-1.
    for (int i = 1; i < ORIG_ROWS - 1; i++) {
        for (int j = 1; j < ORIG_COLS - 1; j++) {
            data_t hw_val = B_hw_array[i][j];
            data_t sw_val = B_sw_array[i][j];

            // Έλεγχος διαφοράς (επειδή είναι float, βάζουμε μια μικρή ανοχή 0.001)
            if (std::abs(hw_val - sw_val) > 0.001f) {
                errors++;
                if (errors <= 5) { // Τυπώνουμε τα 5 πρώτα λάθη για debugging
                    std::cout << "Error at (" << i << "," << j << "): "
                              << "HW = " << hw_val << ", SW = " << sw_val << std::endl;
                }
            }
        }
    }

    // ΒΗΜΑ 6: Αποτέλεσμα Testbench
    if (errors == 0) {
        std::cout << "=======================================" << std::endl;
        std::cout << "  TEST PASSED! 0 errors detected.      " << std::endl;
        std::cout << "=======================================" << std::endl;
        return 0; // Return 0 σημαίνει επιτυχία για το εργαλείο HLS
    } else {
        std::cout << "=======================================" << std::endl;
        std::cout << "  TEST FAILED! " << errors << " errors." << std::endl;
        std::cout << "=======================================" << std::endl;
        return 1; // Return 1 σημαίνει αποτυχία
    }
}