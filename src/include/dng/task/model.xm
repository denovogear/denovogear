/*
 * Copyright (c) 2017 Reed A. Cartwright
 * Authors:  Reed A. Cartwright <reed@cartwrig.ht>
 *
 * This file is part of DeNovoGear.
 *
 * DeNovoGear is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

/***************************************************************************
 *    X-Macro List                                                         *
 *                                                                         *
 * XM((long)(name), (shortname), "description", typename, defaultvalue)    *
 ***************************************************************************/

XM((lib)(bias), , "library/sequencing reference bias (ref/alt ratio)", double, 1.02)
XM((lib)(error), , "library/sequencing error rate (per base-call)", double, DL(0.0005,"0.0005"))
XM((lib)(overdisp), , "library/sequencing overdispersion (pairwise correlation of errors)", double, DL(0.0005,"0.0005"))
XM((mu), , "the germline mutation rate", double, DL(1e-8, "1e-8"))
XM((mu)(somatic), , "the somatic mutation rate", double, DL(0.0, "0"))
XM((mu)(library), , "the library prep mutation rate", double, DL(0.0, "0"))
XM((model), (M), "Inheritance model", std::string, "autosomal")
XM((nuc)(freqs), , "nucleotide frequencies in ACGT order", std::string, "0.3,0.2,0.2,0.3")
XM((ref)(weight), (R), "weight given to reference base for population prior",
   double, DL(1.0, "1"))
XM((theta), , "the population diversity", double, DL(0.001, "0.001"))
