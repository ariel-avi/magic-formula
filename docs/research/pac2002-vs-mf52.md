# PAC2002 vs MF 5.2: difference table

'Same' means the same equation (pac2002.md section 3; mf52.md section 4).

| Aspect | PAC2002 | MF 5.2 | Details |
|---|---|---|---|
| Identification | PROPERTY_FILE_FORMAT 'PAC2002'; FITTYP optional (5 with QSY1 = QSY2 = 0 selects the SHIFTS form of My) | FITTYP = 6 required | model-routing.md |
| Camber in kernels | gamma_x, gamma_y, gamma_z = gamma*LGAX/LGAY/LGAZ (mu_x uses gamma_x, pac2002.md section 4) | raw gamma; no LGA* | pac2002.md section 3; mf52.md section 3 |
| Fx0, Fy0, Ky, SHy, SVy, mu_x, mu_y, Ex, Ey | pure Fx0 and pure Fy0 blocks | same forms | mf52.md section 4, steps 4-5 |
| Bt, Ct, Et, SHt, Br, Dr | pure Mz0 block | same forms; Dr also carries cos(alpha) | mf52.md section 4, step 6 |
| Trail normaliser | R0/FNOMIN in Dt | R0/F'z0 in Dt, times LFZO on t (equal) | pac2002.md section 5.6 |
| cos factor in t | cos(alpha) | cos'(alpha) = Vx/(Vc + eps) | conventions.md section 6 |
| cos factor in Mzr | cos(alpha) on Mzr (pure and combined) | cos(alpha) inside Dr (equal) | mf52.md section 4, steps 6 and 8 |
| F'y | Fy - SVyk | Fy - SVyk0 (mu_y at gamma = 0) | pac2002.md section 5.3; mf52.md section 4, step 8 |
| Moment arm s | R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS | same | pac2002.md section 5.1 |
| Combined method | cosine, or friction ellipse | cosine only | pac2002.md sections 5.4-5.5 |
| Missing R* | none: ellipse; partial: error | 0 with a warning (missing coefficients are not estimated) | pac2002.md section 5.5; policies.md section 3 |
| Mx | Mx equation | same (MF 5.2 has no QSX4-QSX14) | mf52.md section 4, step 9 |
| My | QSY form with QSY2 on Fx and LMY (pac2002.md section 4); output sign flipped; SHIFTS form for FITTYP 5 | same form with a leading minus; no SHIFTS form | conventions.md section 4 |
| Vx | >= 0 | > 0 | conventions.md section 5 |
| Pressure | none | none | conventions.md section 11 |
| Clamping | to [*_RANGE] when both limits exist | same | conventions.md section 8 |
| VXLOW | not applied | not applied | conventions.md section 5 |
| TYRESIDE | no mirroring | no mirroring | conventions.md section 9 |
| Gyroscopic term | Mz_gyr = 0 in steady state | QTZ1, LGYR unused | pac2002.md section 2, step 10 |
| Turn slip | zeta = 1 | no turn-slip parameters | pac2002.md section 1; mf52.md section 3 |
| Transient parameters | out of scope | PTX1-3, PTY1-2, LSGKP, LSGAL unused; out of scope | mf52.md section 3 |
