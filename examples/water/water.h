#ifndef WATER_H
#define WATER_H

struct reaction;

void reaction_init(struct reaction *rxn);

void hydrogen(struct reaction *rxn);

void oxygen(struct reaction *rxn);

void make_water(void);

#endif /* WATER_H */
