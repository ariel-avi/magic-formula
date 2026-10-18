# PAC2002 vs MF 5.2: difference table

Revision 4. 'Same' means the same equation per the cited sources. The MF 5.2 column is [impl] (MFEVAL) unless SIM20 is cited.

| Aspect | PAC2002 | MF 5.2 | Basis |
|---|---|---|---|
| Identification | PROPERTY_FILE_FORMAT 'PAC2002'; FITTYP optional (5 enables the eq-88 path) | FITTYP = 6 required | MSC p.33, p.44, p.49; SIM20 p.26, p.36 |
| Evidence tier | primary | [impl] | README |
| Camber in kernels | gamma_x, gamma_y, gamma_z = gamma*LGAX/LGAY/LGAZ ((20), (32), (49); (23) per C3) | raw gamma; no LGA* | MSC; SIM20 matrix; MFEVAL |
| Fx0, Fy0, Ky, SHy, SVy, mu_x, mu_y, Ex, Ey | (18)-(41) | same forms | mf52.md section 4 |
| Bt, Ct, Et, SHt, Br, Dr | (50)-(56) | same forms; Dr also carries cos(alpha) | MFEVAL :1188, :1263 |
| Trail normaliser | R0/FNOMIN in (52) | R0/F'z0 in Dt, times LFZO on t (equal) | D-23 |
| cos factor in t | cos(alpha) | cos'(alpha) = Vx/(Vc + eps) | conventions.md section 6 |
| cos factor in Mzr | cos(alpha) in (46)/(82) | cos(alpha) inside Dr (equal) | MFEVAL :1263, :1584 |
| F'y | Fy - SVyk | Fy - SVyk computed with mu_y at gamma = 0 | D-16; MFEVAL :1587-1611 |
| Moment arm s | R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS | same | D-09 |
| Combined method | cosine, or the MSC friction ellipse | cosine only | MSC p.33; MFEVAL |
| Missing R* | none -> ellipse; partial -> error | 0 with a warning (TNO estimation not implemented) | D-07; SIM20 p.24-25 |
| Mx | (86) | (86), via eq (49) with QSX4-14 = 0 | MFEVAL :1445 |
| My | (87) with C6, C7; output sign flipped; (88) for FITTYP 5 | same form with a leading minus; no (88) | MFEVAL :1506 |
| Vx | >= 0 | > 0 | D-27 |
| Pressure | none | none | D-25 |
| Clamping | MSC p.45 | SIM20 p.25 (first-time warning) | D-03 |
| VXLOW | not applied | not applied | D-12 |
| TYRESIDE | MSC p.41 | SIM20 p.19 | conventions.md section 9 |
| Gyroscopic term | (100)-(103) -> 0 | QTZ1, LGYR -> 0 | D-20 |
| Turn slip | zeta = 1 | no turn-slip parameters in the file | MSC; SIM20 p.36 |
| Transient parameters | (89)-(99), out of scope | PTX1-3, PTY1-2, LSGKP, LSGAL; out of scope | SIM20 p.21 |
