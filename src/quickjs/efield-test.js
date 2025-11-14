function efield(t) {
    // t should be a float64, in fs
    const cos = Math.cos;
    const sin = Math.sin;

    const ħ  = 0.658212;    // eV*fs
    const hν = 1.71;        // eV
    const ω  = hν / ħ;      //
    const E  = 5.0;       // V/Å

    let x = E * cos(ω*t)
    let y = E * sin(ω*t)
    let z = 0.0;

    return new EField(x, y, z);
}
