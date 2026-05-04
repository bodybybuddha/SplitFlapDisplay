// Flap Generator for splitflap design based on: https://github.com/adamgmakes/SplitFlapDisplay
//
// Flap generator created by Richard Garsthagen (the.anykey@gmail.com)
// License under creative commons: https://creativecommons.org/licenses/by-nc-sa/4.0/

$fn=180; // Quality of render
layers = 3;
layerheight = 0.16;
font = "Consolas:style=Regular";
fontsize = 28;

chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789?!@#$&()-+=;:%'\u20AC\"\u2191\u2193\u20BF\u00b0     []";

// Make the flaps 
MakeFlaps(1);

// Make the letter inlays
MakeFlaps(2);

module MakeFlaps(part){
    for ( y = [0 : 5] ){
        for ( x = [0 : 2 : 12] ){
            char = (y*14)+x;
            if (char<64) {
                if (char==0){
                 translate([17+(x*17),22+(y*43),0])
                 flap(chars[63], chars[char], chars[char+1], part); }
                else if (char==63) {
                 translate([17+(x*17),22+(y*43),0])
                 flap(chars[char-1], chars[char], chars[0], part); }
                else {
                 translate([17+(x*17),22+(y*43),0])
                 flap(chars[char-1], chars[char], chars[char+1], part);} 
            }
        }
    }
}

module flap(c1,c2,c3, part){
    //print flaps with character cutout
    if (part==1){
     difference(){ 
     union(){
     color("black")
     linear_extrude(h=(layers*layerheight))
     import("flap.dxf");
     
     color("black")
     linear_extrude(h=(layers*layerheight))
     rotate([0,0,180])
     import("flap.dxf");
     }
     char1(c1);
     char2(c2);
     char3(c3);
     }
    }

    //print just the characters
    if (part==2){
     char1(c1);
     char2(c2);
     char3(c3);
    }
}

module char1(c){
 difference(){
     color("white")
     translate([0,0,0])
     linear_extrude(h=layerheight)
     rotate([180,0,0])
     text(c, size=fontsize, font=font, halign="center", valign="center");
     
     translate([-20,-0.25,0])
     cube([50,20,layerheight]);
 }
}

module char2(c){
difference(){
     color("white")
     translate([0,0,layerheight*(layers-1)])
     linear_extrude(h=layerheight)
     text(c, size=fontsize, font=font, halign="center", valign="center");
     
     translate([-20,-0.25,layerheight*(layers-1)])
     cube([50,0.5,layerheight]);
     
}
}

module char3(c){
 difference(){
     color("white")
     translate([0,0.25,0])
     linear_extrude(h=layerheight)
     rotate([180,0,0])
     text(c, size=fontsize, font=font, halign="center", valign="center");
     
     translate([-20,-20+0.25,0])
     cube([50,20,layerheight]);
 }
}




