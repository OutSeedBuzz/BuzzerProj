// ============================================================
//   BOITIER BUZZER QUIZ COMPLET — v59 (TOLÉRANCE PERÇAGE LED)
// ============================================================

$fn = 70;

// --- Dimensions du Boîtier ---
B_L = 210;   
B_W = 100;   
B_H = 55;    
B_T = 2.5;   
B_R = 10;    

// --- Dimensions du Dôme ---
D_R      = 28;   
D_H      = 28;   
D_TOL    = 0.5;  
JUPE_H   = 4;    
PISTON_R = 4;    
PISTON_H = 26.5; 

// --- Support Bouton Poussoir ---
BTN_W    = 12.2; 
SUP_H    = 23;   

// --- Interrupteur ON/OFF ---
SW_W   = 29.16;  
SW_H   = 10.31;  
SW_TOL = 0.3;    

// --- Composants Électroniques & Tolérances ---
LED_D   = 5.03;   // Diamètre physique exact de ton composant LED
LED_TOL = 0.12;   // Tolérance d'impression 3D (jeu radial/diamétral pour insertion)
LED_R   = (LED_D + LED_TOL) / 2; // Rayon final du trou ajusté (2.575 mm)

VIS_R   = 1.5;    

// --- Dimensions Prise USB-C ---
USBC_W = 10.5;   
USBC_H = 5.0;    

// --- Module boîte de base ---
module boite_arrondie(l, w, h, r) {
    hull()
        for(x=[r, l-r], y=[r, w-r])
            translate([x, y, 0])
                cylinder(r=r, h=h);
}

// --- Module pour la découpe USB-C ---
module trou_usbc_arrondi(w, h, ep) {
    r = h / 2;
    rotate([0, 90, 0])
    hull() {
        translate([0, r, 0]) cylinder(r=r, h=ep);
        translate([0, w - r, 0]) cylinder(r=r, h=ep);
    }
}

// ============================================================
// 1. LE CORPS DU BOÎTIER
// ============================================================
module corps() {
    difference() {
        union() {
            difference() {
                boite_arrondie(B_L, B_W, B_H, B_R);
                translate([B_T, B_T, B_T])
                    boite_arrondie(B_L-2*B_T, B_W-2*B_T, B_H + 1, B_R-B_T);
            }
            for(x=[12, B_L-12], y=[12, B_W-12])
                translate([x, y, B_T])
                    cylinder(r=5, h=B_H - B_T);
        }

        for(x=[12, B_L-12], y=[12, B_W-12])
            translate([x, y, B_T + 4])
                cylinder(r=VIS_R, h=B_H); 

        // Trous des LEDs en façade avant avec le rayon ajusté + tolérance
        for(lx=[B_L/2-25, B_L/2+25])
            translate([lx, -0.1, 15])
                rotate([-90,0,0])
                    cylinder(r=LED_R, h=B_T+1);

        translate([-0.5, B_W/2 - USBC_W/2, B_H - B_T - 12])
            trou_usbc_arrondi(USBC_W, USBC_H, B_T + 1);

        translate([B_L-B_T-0.1, B_W/2 - (SW_W + SW_TOL)/2, B_H/2 - (SW_H + SW_TOL)/2])
            cube([B_T+0.2, SW_W + SW_TOL, SW_H + SW_TOL]);
            
        translate([B_L-B_T-6, B_W/2 - (SW_W + SW_TOL + 4)/2, B_H/2 - (SW_H + SW_TOL + 4)/2])
            cube([6.1, SW_W + SW_TOL + 4, SW_H + SW_TOL + 4]);

        translate([B_T-0.4, B_T-0.4, B_H - 4])
            boite_arrondie(B_L-2*B_T+0.8, B_W-2*B_T+0.8, 4.1, B_R-B_T+0.5);

        translate([B_L/2, B_W/2, -0.5])
            cylinder(r=D_R + D_TOL, h=B_T + 1);
    }
}

// ============================================================
// 2. LE DÔME DU BUZZER
// ============================================================
module dome() {
    union() {
        translate([0, 0, JUPE_H])
            intersection() {
                scale([1, 1, D_H/D_R]) sphere(r=D_R);
                cylinder(r=D_R+1, h=D_H+1);
            }
        cylinder(r=D_R, h=JUPE_H);
        translate([0, 0, -2.5]) cylinder(r=D_R + 3, h=2.5);
        translate([0, 0, -5.5]) cylinder(r=PISTON_R + 3, h=3);
        translate([0, 0, -PISTON_H]) cylinder(r=PISTON_R, h=PISTON_H);
    }
}

// ============================================================
// 3. LE RESSORT HÉLICOÏDAL CORRIGÉ
// ============================================================
module ressort() {
    S_H       = 26;       
    S_DI      = 9.2;      
    S_DE      = 15.2;     
    S_TOURS   = 4.5;      
    S_EPAIS   = 1.5;      
    
    S_LARGEUR = (S_DE - S_DI) / 2; 

    union() {
        difference() {
            cylinder(r = S_DE/2, h = S_EPAIS);
            translate([0, 0, -0.1]) cylinder(r = S_DI/2, h = S_EPAIS + 0.2);
        }
        
        linear_extrude(height = S_H, twist = -S_TOURS * 360, slices = S_TOURS * 60)

            translate([(S_DI + S_LARGEUR) / 2, 0, 0])
                square([S_LARGEUR, S_EPAIS], center = true);
        
        translate([0, 0, S_H])
            difference() {
                cylinder(r = S_DE/2, h = S_EPAIS);
                translate([0, 0, -0.1]) cylinder(r = S_DI/2, h = S_EPAIS + 0.2);
            }
    }
}

// ============================================================
// 4. LE COUVERCLE INFÉRIEUR
// ============================================================
module couvercle() {
    p_y = BTN_W + 4; 

    difference() {
        union() {
            boite_arrondie(B_L, B_W, B_T, B_R);
            
            translate([B_L/2, B_W/2, B_T]) {
                translate([-BTN_W/2, -p_y/2, 0])
                    cube([BTN_W, p_y, SUP_H]);
            }
        }
        
        translate([B_L/2 - BTN_W/2 - 0.1, B_W/2 - BTN_W/2, B_T - 0.1])
            cube([BTN_W + 0.2, BTN_W, SUP_H - 3 + 0.1]);
        
        translate([B_L/2 - BTN_W/2 - 0.1, B_W/2 - BTN_W/2, B_T + SUP_H - 1.5])
            cube([BTN_W + 0.2, BTN_W, 1.6]);
        
        translate([B_L/2 - 3, B_W/2 - 3, B_T + SUP_H - 4])
            cube([6, 6, 4.5]);

        for(x=[12, B_L-12], y=[12, B_W-12]) {
            translate([x, y, -0.1]) cylinder(r=VIS_R + 0.3, h=B_T+0.2); 
            translate([x, y, B_T - 1.2]) cylinder(r=3.1, h=1.5); 
        }
    }
    
    translate([B_T, B_T, B_T])
        difference() {
            boite_arrondie(B_L-2*B_T, B_W-2*B_T, 3.5, B_R-B_T);
            translate([2, 2, -0.1]) boite_arrondie(B_L-2*B_T-4, B_W-2*B_T-4, 3.8, B_R-B_T-2);
        }
}

// ============================================================
//   DISPOSITION SUR LE PLATEAU
// ============================================================
translate([0, 0, 0]) corps();
translate([0, B_W + 30, 0]) couvercle();
translate([B_L/2 - 40, (B_W * 2) + 60, PISTON_H]) dome();
translate([B_L/2 + 40, (B_W * 2) + 60, 0]) ressort();