// Rendu 3D des boutons du N73. Chaque pièce est un relief vu de dessus : profil tourné (points rayon/hauteur,
// arêtes nettes) éventuellement lobé ou moleté, ou levier rainuré. Éclairage : lumière ponctuelle douce en haut à
// gauche (ombres portées, occlusion) + reflets d'un studio (grande boîte à lumière, bande à droite) vus en perspective.
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

public class Part
{
    public Func<double, double, double> H;          // hauteur au point (x, y) relatif au centre, -1 hors de la pièce
    public double[] Alb;                            // couleur (linéaire)
    public double F0;                               // réflectance de face
    public Func<double, double, double[]> Paint;    // marquage peint (ou null)
}

public static class KnobRender
{
    public static double[] Hex(string h)
    {
        h = h.TrimStart('#');
        double[] c = new double[3];
        for (int i = 0; i < 3; ++i) c[i] = Math.Pow(Convert.ToInt32(h.Substring(i * 2, 2), 16) / 255.0, 2.2);
        return c;
    }

    // profil tourné : pts = { r0, z0, r1, z1, ... } du bord extérieur vers le centre (r décroissant ; même r = paroi)
    static double Lathe(double[] pts, double r)
    {
        if (r > pts[0]) return -1;
        for (int i = 0; i + 3 < pts.Length; i += 2)
        {
            double ra = pts[i], za = pts[i + 1], rb = pts[i + 2], zb = pts[i + 3];
            if (ra <= rb) continue;
            if (r <= ra && r >= rb) return zb + (za - zb) * (r - rb) / (ra - rb);
        }
        return pts[pts.Length - 1];
    }

    // creux entre lobes / stries : 0 sur le relief, 1 au fond du creux
    static double Notch(double th, int n, int kind)
    {
        double t = th * n / (2 * Math.PI); t -= Math.Floor(t);
        if (kind == 0) return 0.5 + 0.5 * Math.Cos(2 * Math.PI * t);         // moletage fin
        return Math.Pow(1 - Math.Sin(Math.PI * t), 1.6);                        // gros lobes arrondis
    }

    public static Part Turned(double[] pts, int n, double depth, int kind, double bandIn, string col, double f0, double rotDeg = 0)
    {
        Part p = new Part(); p.Alb = Hex(col); p.F0 = f0;
        double rot = rotDeg * Math.PI / 180;
        p.H = (x, y) =>
        {
            double r = Math.Sqrt(x * x + y * y);
            if (n > 0 && r > bandIn - 1.5)
            {
                double w = Math.Min(1.0, (r - (bandIn - 1.5)) / 1.5);
                r += depth * w * Notch(Math.Atan2(y, x) - rot, n, kind);
            }
            return Lathe(pts, r);
        };
        return p;
    }

    // levier sur tout le diamètre : dessus plat rainuré au centre, petits chanfreins, bouts carrés
    public static Part Lever(double angDeg, double L, double w, double h, double groove, string col, double f0)
    {
        Part p = new Part(); p.Alb = Hex(col); p.F0 = f0;
        double a = angDeg * Math.PI / 180, sa = Math.Sin(a), ca = Math.Cos(a);
        const double c = 1.1;
        p.H = (x, y) =>
        {
            double u = x * sa - y * ca, v = x * ca + y * sa;
            double au = Math.Abs(u), av = Math.Abs(v);
            if (au > L || av > w / 2) return -1;
            double zv = av > w / 2 - c ? h - 1.3 * (av - (w / 2 - c)) / c : av < groove ? h - 0.9 * (1 - av / groove) : h;
            double zu = au > L - c ? h - 1.3 * (au - (L - c)) / c : h;
            return Math.Min(zv, zu);
        };
        return p;
    }

    public static Func<double, double, double[]> Stripe(double angDeg, double u0, double u1, double v0, double v1, string col, Func<double, double, double[]> next)
    {
        double a = angDeg * Math.PI / 180, sa = Math.Sin(a), ca = Math.Cos(a);
        double[] c = Hex(col);
        return (x, y) =>
        {
            double u = x * sa - y * ca, v = x * ca + y * sa;
            if (u >= u0 && u <= u1 && v >= v0 && v <= v1) return c;
            return next != null ? next(x, y) : null;
        };
    }

    static double Scene(Part[] parts, double x, double y, out int id)
    {
        double best = Floor; id = -1;
        for (int i = 0; i < parts.Length; ++i) { double z = parts[i].H(x, y); if (z > best) { best = z; id = i; } }
        return best;
    }
    static double Scene(Part[] parts, double x, double y) { int id; return Scene(parts, x, y, out id); }

    static double[] Norm(double x, double y, double z) { double l = Math.Sqrt(x * x + y * y + z * z); return new double[] { x / l, y / l, z / l }; }
    static double Smooth(double a, double b, double x) { double t = Math.Min(1.0, Math.Max(0.0, (x - a) / (b - a))); return t * t * (3 - 2 * t); }

    // hauteur du plan qui reçoit les ombres quand la pièce est rendue seule (0 = panneau)
    public static double Floor = 0;
    const double MaxH = 24, CamZ = 700;
    static readonly double[] LightPos = { -190, -250, 270 };
    static readonly double[] S1 = Norm(-190, -250, 270);          // grande boîte à lumière (même côté que la lumière)
    static readonly double[] S2 = Norm(0.80, -0.30, 0.52);        // bande à droite

    // le studio vu dans les reflets
    static double Env(double[] R)
    {
        double e = 0.04 + 0.10 * Math.Max(0, R[2]);
        double a1 = Math.Acos(Math.Max(-1.0, Math.Min(1.0, R[0] * S1[0] + R[1] * S1[1] + R[2] * S1[2])));
        e += 1.5 * (1 - Smooth(0.20, 0.48, a1));
        double a2 = Math.Acos(Math.Max(-1.0, Math.Min(1.0, R[0] * S2[0] + R[1] * S2[1] + R[2] * S2[2])));
        e += 0.55 * (1 - Smooth(0.07, 0.19, a2));
        e += 0.30 * Smooth(-0.03, 0.05, -(R[0] + R[1]) * 0.7071) * Smooth(0.85, 0.99, R[2]);   // plafond : moitié haut-gauche plus claire
        return e;
    }

    static double Vis(Part[] parts, double x, double y, double z)
    {
        int lit = 0;
        for (int i = 0; i < 9; ++i)
        {
            double jx = 0, jy = 0, jz = 0;
            if (i > 0) { double q = (i - 1) * Math.PI / 4; jx = 30 * Math.Cos(q); jy = 30 * Math.Sin(q) * 0.7; jz = 30 * Math.Sin(q) * 0.7; }
            double lx = LightPos[0] + jx - x, ly = LightPos[1] + jy - y, lz = LightPos[2] + jz - z;
            double hl = Math.Sqrt(lx * lx + ly * ly), dx = lx / hl, dy = ly / hl, tanE = lz / hl;
            bool occ = false;
            for (double t = 0.3; ; t += 0.4)
            {
                double zr = z + 0.05 + t * tanE;
                if (zr > MaxH) break;
                if (Scene(parts, x + dx * t, y + dy * t) > zr) { occ = true; break; }
            }
            if (!occ) ++lit;
        }
        return lit / 9.0;
    }

    static readonly double[] AoDist = { 0.8, 1.8, 3.4, 6.0, 10.0, 15.0 };
    static double Ao(Part[] parts, double x, double y, double z)
    {
        double sum = 0;
        for (int k = 0; k < 12; ++k)
        {
            double a = k * Math.PI / 6 + 0.2, cx = Math.Cos(a), cy = Math.Sin(a), m = 0;
            foreach (double dist in AoDist)
            {
                double hz = Scene(parts, x + cx * dist, y + cy * dist) - z;
                if (hz > 0) m = Math.Max(m, hz / dist);
            }
            sum += Math.Atan(m) / (Math.PI / 2);
        }
        return 1 - 1.0 * sum / 12;
    }

    static double Srgb(double c) { return Math.Pow(Math.Min(1.0, Math.Max(0.0, c)), 1 / 2.2); }

    public static void Render(Part[] parts, double box, double scale, string file)
    {
        int N = (int) Math.Round(box * scale);
        byte[] buf = new byte[N * N * 4];
        Parallel.For(0, N, py =>
        {
            for (int px = 0; px < N; ++px)
            {
                double x0 = (px + 0.5) / scale - box / 2, y0 = (py + 0.5) / scale - box / 2;
                double z0 = Scene(parts, x0, y0);
                double vis = Vis(parts, x0, y0, z0), ao = Ao(parts, x0, y0, z0);
                double sr = 0, sg = 0, sb = 0, sa = 0;
                for (int sy = 0; sy < 3; ++sy)
                    for (int sx = 0; sx < 3; ++sx)
                    {
                        double x = (px + (sx + 0.5) / 3) / scale - box / 2, y = (py + (sy + 0.5) / 3) / scale - box / 2;
                        int id; double z = Scene(parts, x, y, out id);
                        if (id < 0)
                        {
                            sa += Math.Min(1.0, (1 - ao * (0.3 + 0.7 * vis)) * 1.05);   // ombre sur le panneau (noir translucide)
                            continue;
                        }
                        Part p = parts[id];
                        const double e = 0.12;
                        double hxp = p.H(x + e, y), hxm = p.H(x - e, y), hyp = p.H(x, y + e), hym = p.H(x, y - e);
                        double dx = hxp >= 0 && hxm >= 0 ? (hxp - hxm) / (2 * e) : hxp >= 0 ? (hxp - z) / e : hxm >= 0 ? (z - hxm) / e : 0;
                        double dy = hyp >= 0 && hym >= 0 ? (hyp - hym) / (2 * e) : hyp >= 0 ? (hyp - z) / e : hym >= 0 ? (z - hym) / e : 0;
                        double[] n = Norm(-dx, -dy, 1);
                        double[] alb = p.Alb;
                        bool painted = false;
                        if (p.Paint != null) { double[] pc = p.Paint(x, y); if (pc != null) { alb = pc; painted = true; } }
                        double[] L = Norm(LightPos[0] - x, LightPos[1] - y, LightPos[2] - z);
                        double[] V = Norm(-x, -y, CamZ - z);
                        double nv = Math.Max(0.0, n[0] * V[0] + n[1] * V[1] + n[2] * V[2]);
                        double[] R = { 2 * nv * n[0] - V[0], 2 * nv * n[1] - V[1], 2 * nv * n[2] - V[2] };
                        double f0 = painted ? 0.04 : p.F0;
                        double F = f0 + (1 - f0) * Math.Pow(1 - nv, 5);
                        double ndl = Math.Max(0, n[0] * L[0] + n[1] * L[1] + n[2] * L[2]);
                        double k = (0.10 + 0.16 * n[2]) * ao + 0.95 * ndl * (0.12 + 0.88 * vis);
                        double refl = F * Env(R) * (0.35 + 0.65 * ao) * (0.55 + 0.45 * vis);
                        sr += Srgb(alb[0] * k * (1 - F) + refl); sg += Srgb(alb[1] * k * (1 - F) + refl); sb += Srgb(alb[2] * k * (1 - F) + refl);
                        sa += 1;
                    }
                int o = (py * N + px) * 4;
                if (sa > 0)
                {
                    buf[o] = (byte) Math.Round(255 * Math.Min(1.0, sb / sa));
                    buf[o + 1] = (byte) Math.Round(255 * Math.Min(1.0, sg / sa));
                    buf[o + 2] = (byte) Math.Round(255 * Math.Min(1.0, sr / sa));
                    buf[o + 3] = (byte) Math.Round(255 * Math.Min(1.0, sa / 9));
                }
            }
        });
        using (Bitmap bmp = new Bitmap(N, N, PixelFormat.Format32bppArgb))
        {
            BitmapData d = bmp.LockBits(new Rectangle(0, 0, N, N), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            for (int row = 0; row < N; ++row) Marshal.Copy(buf, row * N * 4, d.Scan0 + row * d.Stride, N * 4);
            bmp.UnlockBits(d);
            bmp.Save(file, ImageFormat.Png);
        }
    }

    // capuchon à rebord (GAIN, HIGH PASS) : rebord surélevé, rainure sombre, dessus plat en retrait
    static double[] RimCap(double r0, double rim, double h)
    {
        double ri = r0 - rim;
        return new double[] { r0, 0, r0, h - 1.6, r0 - 0.8, h, ri + 0.5, h + 0.5, ri, h - 0.6, ri - 0.5, h - 1.2, ri - 1.0, h - 1.2, ri - 1.5, h - 0.5, 0, h - 0.3 };
    }
    // bouton gris du 1073 : couronne de lobes, rainure, dessus plat
    static double[] GreyCap(double r0, double rTop, double h)
    {
        return new double[] { r0, 0, r0, h - 5.5, r0 - 0.8, h - 4.2, rTop + 1.6, h - 2.6, rTop + 1.0, h - 3.2, rTop + 0.4, h - 3.2, rTop, h - 0.7, rTop - 0.7, h, 0, h + 0.3 };
    }
    // jupe conique fumée sous les leviers
    static double[] Skirt(double r0, double rIn, double h)
    {
        return new double[] { r0, 0, r0, 1.4, r0 - 0.9, 2.3, rIn + 0.8, h, rIn, h + 0.2, 0, h + 0.2 };
    }

    // Les six boutons du N73, aux positions de la maquette
    public static void RenderAll(string dir, double scale)
    {
        const string red = "#680e11", smoke = "#4a4c52", smokeDark = "#303036", grey = "#86878d", cream = "#dcd1b5", navy = "#142456", black = "#19181b";
        const string paint = "#f4f0e8", ember = "#e5384a";

        // GAIN : levier rouge rainuré sur jupe fumée, capuchon rouge à rebord — MIC 60 (−60°)
        Part lever = Lever(-60, 59, 12.5, 11, 1.5, red, 0.06);
        lever.Paint = Stripe(-60, 43, 57.5, -1.3, 1.3, paint, null);
        Render(new Part[] { Turned(Skirt(55.5, 41, 7.5), 0, 0, 0, 0, smoke, 0.08), lever, Turned(RimCap(37, 6, 16), 0, 0, 0, 0, red, 0.06) },
               164, scale, dir + "\\gain.png");

        // HIGH : bouton gris à lobes sur jupe sombre — +4 dB
        Part hcap = Turned(GreyCap(35.5, 26.5, 15), 20, 2.1, 1, 28, grey, 0.06);
        hcap.Paint = Stripe(209.5, 24.5, 35, -1.3, 1.3, paint, null);
        Render(new Part[] { Turned(Skirt(48, 36.5, 6.5), 0, 0, 0, 0, smokeDark, 0.08), hcap }, 136, scale, dir + "\\high.png");

        // MID : bague crème moletée (3·2 kHz, 210°) + bouton gris (−4 dB)
        double[] ring = { 54, 0, 54, 5, 53.3, 6, 48, 7.2, 40.6, 8.6, 40.1, 9.5, 39.6, 9.5, 39.2, 7, 38.8, 3, 0, 3 };
        Part mring = Turned(ring, 120, 0.6, 0, 48, cream, 0.05);
        mring.Paint = Stripe(210, 41.5, 53, -1.6, 1.6, ember, null);
        Part mcap = Turned(GreyCap(38.5, 27.5, 16), 20, 2.1, 1, 29, grey, 0.06);
        mcap.Paint = Stripe(153.8, 25.5, 38, -1.3, 1.3, paint, null);
        Render(new Part[] { mring, mcap }, 148, scale, dir + "\\mid.png");

        // LOW : 110 Hz (180°), −4 dB
        Part lring = Turned(ring, 120, 0.6, 0, 48, cream, 0.05);
        lring.Paint = Stripe(180, 41.5, 53, -1.6, 1.6, ember, null);
        Part lcap = Turned(GreyCap(38.5, 27.5, 16), 20, 2.1, 1, 29, grey, 0.06);
        lcap.Paint = Stripe(150.5, 25.5, 38, -1.3, 1.3, paint, null);
        Render(new Part[] { lring, lcap }, 148, scale, dir + "\\low.png");

        // HIGH PASS : levier bleu nuit rainuré — 80 Hz (150°)
        Part hlever = Lever(150, 54, 14, 10.5, 1.6, navy, 0.06);
        hlever.Paint = Stripe(150, 39, 53.5, -1.3, 1.3, paint, null);
        Render(new Part[] { Turned(Skirt(54, 40, 7), 0, 0, 0, 0, smoke, 0.08), hlever, Turned(RimCap(38, 5, 15.5), 0, 0, 0, 0, navy, 0.06) },
               148, scale, dir + "\\hpf.png");

        // OUTPUT : bouton noir à lobes — 0 dB (en haut)
        Part ocap = Turned(GreyCap(27, 18.5, 12), 20, 2.4, 1, 20, black, 0.07);
        ocap.Paint = Stripe(0, 9, 26, -1.2, 1.2, paint, null);
        Render(new Part[] { ocap }, 100, scale, dir + "\\output.png");
    }

    // Les images du plugin (Resources/knobs) : leviers à chaque cran, pièces tournantes par pas de 1,5° sur une
    // période de leurs lobes (18°), bague crème seule. Les repères blancs et l'encoche braise sont dessinés par le code.
    public static void RenderAssets(string dir, double scale)
    {
        const string red = "#680e11", smoke = "#4a4c52", smokeDark = "#303036", grey = "#86878d", cream = "#dcd1b5", navy = "#142456", black = "#19181b";
        const string paint = "#f4f0e8";

        Floor = 0;
        for (int i = 0; i < 22; ++i)
        {
            double a = 45 + 15 * i;
            Part lever = Lever(a, 59, 12.5, 11, 1.5, red, 0.06);
            lever.Paint = Stripe(a, 43, 57.5, -1.3, 1.3, paint, null);
            Render(new Part[] { Turned(Skirt(55.5, 41, 7.5), 0, 0, 0, 0, smoke, 0.08), lever, Turned(RimCap(37, 6, 16), 0, 0, 0, 0, red, 0.06) },
                   164, scale, dir + "\\gain_" + i.ToString("00") + ".png");
        }
        for (int i = 0; i < 5; ++i)
        {
            double a = 90 + 30 * i;
            Part hlever = Lever(a, 54, 14, 10.5, 1.6, navy, 0.06);
            hlever.Paint = Stripe(a, 39, 53.5, -1.3, 1.3, paint, null);
            Render(new Part[] { Turned(Skirt(54, 40, 7), 0, 0, 0, 0, smoke, 0.08), hlever, Turned(RimCap(38, 5, 15.5), 0, 0, 0, 0, navy, 0.06) },
                   148, scale, dir + "\\hpf_" + i + ".png");
        }
        Render(new Part[] { Turned(Skirt(48, 36.5, 6.5), 0, 0, 0, 0, smokeDark, 0.08) }, 136, scale, dir + "\\high_base.png");
        double[] ring = { 54, 0, 54, 5, 53.3, 6, 48, 7.2, 40.6, 8.6, 40.1, 9.5, 39.6, 9.5, 39.2, 7, 38.8, 3, 0, 3 };
        Render(new Part[] { Turned(ring, 120, 0.6, 0, 48, cream, 0.05) }, 148, scale, dir + "\\ring.png");
        for (int k = 0; k < 12; ++k)
        {
            double rot = 1.5 * k;
            Floor = 6.7;   // l'ombre du capuchon tombe sur la jupe
            Render(new Part[] { Turned(GreyCap(35.5, 26.5, 15), 20, 2.1, 1, 28, grey, 0.06, rot) }, 136, scale, dir + "\\high_cap_" + k.ToString("00") + ".png");
            Floor = 9.0;   // … sur la bague crème
            Render(new Part[] { Turned(GreyCap(38.5, 27.5, 16), 20, 2.1, 1, 29, grey, 0.06, rot) }, 148, scale, dir + "\\cap_" + k.ToString("00") + ".png");
            Floor = 0;
            Render(new Part[] { Turned(GreyCap(27, 18.5, 12), 20, 2.4, 1, 20, black, 0.07, rot) }, 100, scale, dir + "\\out_" + k.ToString("00") + ".png");
        }
        Floor = 0;
    }
}
