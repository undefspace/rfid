$fn = 100;

pn532_w = 43;
pn532_h = 40.5;

pn532_screws_w = 28;
pn532_screws_h = 26;
pn532_screws_offs_x = 7.5;
pn532_screws_offs_y = 7.3;

pn532_pins_offs_x = 13.2;
pn532_pins_offs_y = 5.25;
pn532_pins_w = 21.5;
pn532_pins_h = 2.5;
pn532_pins_t = 1.5;

panel_t = 3;
side_panel_h = 15;
stiffener = 7;
thread_indent_t = 3.2;

// top panel
translate(v = [panel_t, 0, 0]) 
difference() {
    cube(size = [pn532_w, pn532_h + (2 * panel_t), panel_t]);

    translate(v = [0, panel_t, 0]) { 
        // pin cavity
        translate(v = [pn532_pins_offs_x, pn532_pins_offs_y, 0]) 
            cube(size = [pn532_pins_w, pn532_pins_h, pn532_pins_t]);

        // thread indents
        for(i = [0, 1])
        translate(v = [pn532_screws_offs_x + (i * pn532_screws_w), pn532_screws_offs_y + (i * pn532_screws_h), 0])
            cylinder(h = panel_t * 2, r = thread_indent_t / 2, center=true);
    }
}

// side panel
side_panel_w = pn532_h + (2 * panel_t);
translate(v = [0, 0, -side_panel_h])
difference() {
    cube(size = [panel_t, side_panel_w, side_panel_h + panel_t]);

    // thread indents
    for(i = [side_panel_w / 4, side_panel_w * 3 / 4])
    translate(v = [0, i, side_panel_h / 2]) 
    rotate(a = [0, -90, 0])
        cylinder(h = panel_t * 2, r = thread_indent_t / 2, center=true);
}

// stiffeners
for(y = [0, pn532_h + panel_t])
translate(v = [panel_t, y, 0])
rotate(a = [-90, 0, 0]) 
linear_extrude(height = panel_t)
    polygon(points=[[0, 0], [stiffener, 0], [0, stiffener]]);
