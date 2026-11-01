// The n-body simulation: the C baseline of programs/nbody.at.
#include <math.h>
#include <stdio.h>

#define PI 3.141592653589793
#define SOLAR_MASS (4 * PI * PI)
#define DAYS 365.24
#define N 5

typedef struct {
    double x, y, z, vx, vy, vz, mass;
} Body;

static Body body(double x, double y, double z, double vx, double vy, double vz, double mass) {
    return (Body){x, y, z, vx * DAYS, vy * DAYS, vz * DAYS, mass * SOLAR_MASS};
}

static void offset_momentum(Body *b) {
    double px = 0, py = 0, pz = 0;
    for (int i = 0; i < N; i++) {
        px += b[i].vx * b[i].mass;
        py += b[i].vy * b[i].mass;
        pz += b[i].vz * b[i].mass;
    }
    b[0].vx = -px / b[0].mass;
    b[0].vy = -py / b[0].mass;
    b[0].vz = -pz / b[0].mass;
}

static double energy(const Body *b) {
    double e = 0;
    for (int i = 0; i < N; i++) {
        e += 0.5 * b[i].mass * (b[i].vx * b[i].vx + b[i].vy * b[i].vy + b[i].vz * b[i].vz);
        for (int j = i + 1; j < N; j++) {
            double dx = b[i].x - b[j].x, dy = b[i].y - b[j].y, dz = b[i].z - b[j].z;
            e -= b[i].mass * b[j].mass / sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    return e;
}

static void advance(Body *b, double dt) {
    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            double dx = b[i].x - b[j].x, dy = b[i].y - b[j].y, dz = b[i].z - b[j].z;
            double d2 = dx * dx + dy * dy + dz * dz;
            double mag = dt / (d2 * sqrt(d2));
            double mi = b[i].mass * mag, mj = b[j].mass * mag;
            b[i].vx -= dx * mj;
            b[i].vy -= dy * mj;
            b[i].vz -= dz * mj;
            b[j].vx += dx * mi;
            b[j].vy += dy * mi;
            b[j].vz += dz * mi;
        }
    }
    for (int i = 0; i < N; i++) {
        b[i].x += dt * b[i].vx;
        b[i].y += dt * b[i].vy;
        b[i].z += dt * b[i].vz;
    }
}

int main(void) {
    Body b[N] = {
        {0, 0, 0, 0, 0, 0, SOLAR_MASS},
        body(4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
             0.00166007664274403694, 0.00769901118419740425, -0.0000690460016972063023,
             0.000954791938424326609),
        body(8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
             -0.00276742510726862411, 0.00499852801234917238, 0.0000230417297573763929,
             0.000285885980666130812),
        body(12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
             0.00296460137564761618, 0.00237847173959480950, -0.0000296589568540237556,
             0.0000436624404335156298),
        body(15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
             0.00268067772490389322, 0.00162824170038242295, -0.0000951592254519715870,
             0.0000515138902046611451),
    };
    offset_momentum(b);
    printf("%.17g\n", energy(b));
    for (int k = 0; k < 5000000; k++) {
        advance(b, 0.01);
    }
    printf("%.17g\n", energy(b));
    return 0;
}
