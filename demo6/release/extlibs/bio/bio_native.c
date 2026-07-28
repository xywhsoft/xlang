/*
 * bio_native.c - Bioinformatics toolkit
 * Tier 4: biology
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xbio_codon(long long id) {
    switch (id) {
        case 0: return "AUG";  /* Met/Start */
        case 1: return "UAA";  /* Stop */
        case 2: return "UAG";  /* Stop */
        case 3: return "UGA";  /* Stop */
        case 4: return "UUU";  /* Phe */
        case 5: return "GCU";  /* Ala */
        case 6: return "GGU";  /* Gly */
        case 7: return "UCU";  /* Ser */
        default: return "Unknown";
    }
}

const char* xbio_amino_acid(long long id) {
    switch (id) {
        case 0: return "Ala";
        case 1: return "Arg";
        case 2: return "Asn";
        case 3: return "Asp";
        case 4: return "Cys";
        case 5: return "Glu";
        case 6: return "Gly";
        case 7: return "His";
        case 8: return "Ile";
        case 9: return "Leu";
        case 10: return "Lys";
        case 11: return "Met";
        case 12: return "Phe";
        case 13: return "Pro";
        case 14: return "Ser";
        case 15: return "Thr";
        case 16: return "Trp";
        case 17: return "Tyr";
        case 18: return "Val";
        default: return "Unknown";
    }
}

/* Complement DNA base */
const char* xbio_complement(const char *seq) {
    int i = 0;
    while (seq[i] && i < 500) {
        switch (seq[i]) {
            case 'A': buf[i] = 'T'; break;
            case 'T': buf[i] = 'A'; break;
            case 'G': buf[i] = 'C'; break;
            case 'C': buf[i] = 'G'; break;
            case 'a': buf[i] = 't'; break;
            case 't': buf[i] = 'a'; break;
            case 'g': buf[i] = 'c'; break;
            case 'c': buf[i] = 'g'; break;
            default: buf[i] = seq[i]; break;
        }
        i++;
    }
    buf[i] = '\0';
    return buf;
}

/* Transcribe DNA to RNA (T->U) */
const char* xbio_transcribe(const char *dna) {
    int i = 0;
    while (dna[i] && i < 500) {
        if (dna[i] == 'T') buf[i] = 'U';
        else if (dna[i] == 't') buf[i] = 'u';
        else buf[i] = dna[i];
        i++;
    }
    buf[i] = '\0';
    return buf;
}

/* GC content percentage x100 */
long long xbio_gc_content(const char *seq) {
    long long gc = 0, total = 0;
    const char *p = seq;
    while (*p) {
        if (*p == 'G' || *p == 'C' || *p == 'g' || *p == 'c') gc++;
        if (*p == 'A' || *p == 'T' || *p == 'G' || *p == 'C' ||
            *p == 'a' || *p == 't' || *p == 'g' || *p == 'c') total++;
        p++;
    }
    if (total == 0) return 0;
    return (gc * 10000) / total;
}

const char* xbio_format(long long id) {
    switch (id) {
        case 0: return "FASTA";
        case 1: return "FASTQ";
        case 2: return "GenBank";
        case 3: return "SAM";
        case 4: return "BAM";
        case 5: return "VCF";
        case 6: return "BED";
        case 7: return "GFF";
        default: return "Unknown";
    }
}
