#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define N_PIX 784 //numero dei pixel presenti nelle immagini
#define N_EPOCH 6 //numero delle epoche
#define N_CLASSI 10 //numero delle classi
#define LR 1   //Learning Rate

int *check_mem_vett(int size); //controlla l'allocazione di memoria e restituisce il vettore allocato
int leggi_riga(FILE *f, int *out1); //leggere la riga dal file e la mette nell'array
void init_pesi(int *W, int *b); //inizializza i pesi
void forward(int *img, int *W, int *b, int *score); //calcola il punteggio di ogni classe a partire dall'immagine
void allena_epoca(int *W, int *b, int *out1); // esegue l'addestramento del percettrone per un'epoca
void valuta(int *W, int *b, int *out1); //valuta l'accuratezza del percettrone sul dataset di test
void salva_pesi(int *W, int *b); //salva i pesi e i bias su file
int carica_pesi(int *W, int *b); //carica da file i pesi e i bias precedentemente salvati

int main(int argc, char **argv) {
    consoleDemoInit();

    int *out1 = check_mem_vett(N_PIX);
	
	//inizializza il file system FAT
    if (!fatInitDefault()) {
        iprintf("fatInitDefault failure: terminating\n");
        free(out1);
        return 1;
    }

    int *W;          
    int b[N_CLASSI];
    W = malloc(sizeof(int) * N_CLASSI * N_PIX);
    if (W == NULL) {
        iprintf("Errore allocazione W\n");
        free(out1);
        return 1;
    }

    init_pesi(W, b);

    DIR *pdir = opendir("/array/outputTrain");
    if (!pdir) {
        iprintf("opendir() failure; terminating\n");
        free(out1);
        free(W);
        return 1;
    }
    iprintf("Avvio training...\n");

	
	for (int epoca = 0; epoca < N_EPOCH; epoca++) {
		allena_epoca(W, b, out1);
		iprintf("Epoca %d completata\n", epoca);
	}
    iprintf("Valutazione...\n");
    valuta(W, b, out1);
    iprintf("Salvataggio dati...");
    salva_pesi(W, b);
    iprintf("Salvataggio file effettuato...\n");
    closedir(pdir);

    while (pmMainLoop()) {
        swiWaitForVBlank();
        scanKeys();
        if (keysDown() & KEY_START) break;
    }

	//libero memoria
    free(out1);
    free(W);
    fflush(stdout);
    return 0;
}

int *check_mem_vett(int size) {
    int *a = malloc(sizeof(int) * size);
    if (a == NULL) {
        iprintf("Errore allocazione memoria\n");
        exit(1);
    }
    return a;
}

// legge una riga di N_PIX interi separati da spazio in out1
int leggi_riga(FILE *f, int *out1) {
    for (int i = 0; i < N_PIX; i++) {
        if (fscanf(f, "%d", &out1[i]) != 1)
            return 0; // fine file o errore
    }
    return 1;
}

void init_pesi(int *W, int *b) {
    for (int c = 0; c < N_CLASSI; c++) {
        b[c] = 0;
        for (int p = 0; p < N_PIX; p++)
            W[c * N_PIX + p] = 0;
    }
}

void forward(int *img, int *W, int *b, int *score) {
    for (int c = 0; c < N_CLASSI; c++) {
        int s = b[c];
        for (int p = 0; p < N_PIX; p++)
            s += W[c * N_PIX + p] * img[p];
        score[c] = s;
    }
}
void allena_epoca(int *W, int *b, int *out1) {
    FILE *file[N_CLASSI];
    int attivo[N_CLASSI]; // 1 se il file ha ancora righe da leggere

    for (int c = 0; c < N_CLASSI; c++) {
        char percorso[32];
        sprintf(percorso, "/output/output%d.txt", c);
        file[c] = fopen(percorso, "r");
        attivo[c] = (file[c] != NULL);
        if (!attivo[c]) iprintf("Impossibile aprire classe %d\n", c);
    }

    int rimangono = 1;
    while (rimangono) {
        rimangono = 0;
        for (int classe = 0; classe < N_CLASSI; classe++) {
            if (!attivo[classe]) continue;

            if (leggi_riga(file[classe], out1)) {
                rimangono = 1;

                int score[N_CLASSI];
                forward(out1, W, b, score);

                int pred = 0;
                for (int c = 1; c < N_CLASSI; c++)
                    if (score[c] > score[pred]) pred = c;

                if (pred != classe) {
                    for (int p = 0; p < N_PIX; p++) {
                        W[classe * N_PIX + p] += LR * out1[p];
                        W[pred   * N_PIX + p] -= LR * out1[p];
                    }
                    b[classe] += LR;
                    b[pred]   -= LR;
                }
            } else {
                attivo[classe] = 0; // file esaurito
            }
        }
    }

    for (int c = 0; c < N_CLASSI; c++)
        if (file[c]) fclose(file[c]);
}
void valuta(int *W, int *b, int *out1) {
    int corretti = 0;
    int totali = 0;

    DIR *pdir = opendir("/array/outputTest/");
    if (!pdir) {
        iprintf("Impossibile aprire /array/outputTest/\n");
        return;
    }

    struct dirent *pent;
    while ((pent = readdir(pdir)) != NULL) {
        if (strcmp(".", pent->d_name) == 0 || strcmp("..", pent->d_name) == 0)
            continue;
        if (pent->d_type == DT_DIR)
            continue;

        int classe = -1;
        char atteso[32];
        for (int i = 0; i < N_CLASSI; i++) {
            sprintf(atteso, "outputTest%d.txt", i);
            if (strcmp(atteso, pent->d_name) == 0) {
                classe = i;
                break;
            }
        }
        if (classe == -1)
            continue;

        char percorso[255];
        sprintf(percorso, "/array/outputTest/%s", pent->d_name);

        FILE *f = fopen(percorso, "r");
        if (!f) {
            iprintf("Impossibile aprire %s\n", percorso);
            continue;
        }

        while (leggi_riga(f, out1)) {
            int score[N_CLASSI];
            forward(out1, W, b, score);

            int pred = 0;
            for (int c = 1; c < N_CLASSI; c++)
                if (score[c] > score[pred]) pred = c;

            if (pred == classe) corretti++;
            totali++;
        }
        fclose(f);
    }
    closedir(pdir);

    iprintf("Accuratezza: %d/%d", corretti, totali);
    if (totali > 0) {
        int perc = (corretti * 100) / totali; // percentuale intera
        iprintf(" (%d%%)\n", perc);
    } else {
        iprintf("\n");
    }
}
void salva_pesi(int *W, int *b) {
	FILE *f = fopen("/pesi.bin", "wb");
	if (!f) { iprintf("Errore apertura file pesi.bin\n"); return; }
	if (fwrite(W, sizeof(int), N_CLASSI * N_PIX, f) != N_CLASSI * N_PIX) {
		iprintf("Errore scrittura pesi\n"); 
		fclose(f); 
		return;
	}
	if (fwrite(b, sizeof(int), N_CLASSI, f) != N_CLASSI) { 
		iprintf("Errore scrittura b\n"); 
		fclose(f); 
		return; 
	}
    fclose(f);
}
int carica_pesi(int *W, int *b) {
    FILE *f = fopen("/pesi.bin", "rb");
    if (!f) return 0; // non esiste ancora
    fread(W, sizeof(int), N_CLASSI * N_PIX, f);
    fread(b, sizeof(int), N_CLASSI, f);
    fclose(f);
    return 1;
}

