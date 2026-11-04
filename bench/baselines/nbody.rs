// N-body simulation: the Rust baseline of programs/nbody.of.
const PI: f64 = 3.141592653589793;
const SOLAR_MASS: f64 = 4.0 * PI * PI;
const DAYS: f64 = 365.24;

#[derive(Clone, Copy)]
struct Body {
    x: f64, y: f64, z: f64,
    vx: f64, vy: f64, vz: f64,
    mass: f64,
}

fn body(x: f64, y: f64, z: f64, vx: f64, vy: f64, vz: f64, mass: f64) -> Body {
    Body { x, y, z, vx: vx * DAYS, vy: vy * DAYS, vz: vz * DAYS, mass: mass * SOLAR_MASS }
}

fn system() -> [Body; 5] {
    [
        Body { x: 0.0, y: 0.0, z: 0.0, vx: 0.0, vy: 0.0, vz: 0.0, mass: SOLAR_MASS },
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
    ]
}

fn offset_momentum(b: &mut [Body; 5]) {
    let (mut px, mut py, mut pz) = (0.0, 0.0, 0.0);
    for body in b.iter() {
        px += body.vx * body.mass;
        py += body.vy * body.mass;
        pz += body.vz * body.mass;
    }
    b[0].vx = -px / b[0].mass;
    b[0].vy = -py / b[0].mass;
    b[0].vz = -pz / b[0].mass;
}

fn energy(b: &[Body; 5]) -> f64 {
    let mut e = 0.0;
    for i in 0..b.len() {
        let body = b[i];
        e += 0.5 * body.mass * (body.vx * body.vx + body.vy * body.vy + body.vz * body.vz);
        for other in b.iter().skip(i + 1) {
            let dx = body.x - other.x;
            let dy = body.y - other.y;
            let dz = body.z - other.z;
            e -= body.mass * other.mass / (dx * dx + dy * dy + dz * dz).sqrt();
        }
    }
    e
}

fn advance(b: &mut [Body; 5], dt: f64) {
    for i in 0..b.len() {
        for j in i + 1..b.len() {
            let dx = b[i].x - b[j].x;
            let dy = b[i].y - b[j].y;
            let dz = b[i].z - b[j].z;
            let d2 = dx * dx + dy * dy + dz * dz;
            let mag = dt / (d2 * d2.sqrt());
            let mi = b[i].mass * mag;
            let mj = b[j].mass * mag;
            b[i].vx -= dx * mj;
            b[i].vy -= dy * mj;
            b[i].vz -= dz * mj;
            b[j].vx += dx * mi;
            b[j].vy += dy * mi;
            b[j].vz += dz * mi;
        }
    }
    for body in b.iter_mut() {
        body.x += dt * body.vx;
        body.y += dt * body.vy;
        body.z += dt * body.vz;
    }
}

fn main() {
    let mut bodies = system();
    offset_momentum(&mut bodies);
    println!("{:.17}", energy(&bodies));
    for _ in 0..5_000_000 {
        advance(&mut bodies, 0.01);
    }
    println!("{:.17}", energy(&bodies));
}
