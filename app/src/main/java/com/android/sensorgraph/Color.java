package com.android.sensorgraph;

enum Color {
    RED(0, R.id.seekBarR, R.id.barLabelR), GREEN(1, R.id.seekBarG, R.id.barLabelG), BLUE(2, R.id.seekBarB, R.id.barLabelB);
    final int id;
    final int labelId;
    final int sliderId;

    Color(int id, int sliderId, int labelId) {
        this.id = id;
        this.sliderId = sliderId;
        this.labelId = labelId;
    }

    static Color getWithId(int id) {
        for (Color c : values()) {
            if (c.id == id)
                return c;
        }
        return null;
    }
}