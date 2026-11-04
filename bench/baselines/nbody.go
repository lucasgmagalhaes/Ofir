// N-body simulation: the Go baseline of programs/nbody.of.
package main

import (
	"fmt"
	"math"
)

const (
	pi        = 3.141592653589793
	solarMass = 4.0 * pi * pi
	days      = 365.24
)

type Body struct {
	x, y, z    float64
	vx, vy, vz float64
	mass       float64
}

func body(x, y, z, vx, vy, vz, mass float64) Body {
	return Body{x, y, z, vx * days, vy * days, vz * days, mass * solarMass}
}

func system() [5]Body {
	return [5]Body{
		{mass: solarMass},
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
	}
}

func offsetMomentum(b *[5]Body) {
	px, py, pz := 0.0, 0.0, 0.0
	for i := range b {
		px += b[i].vx * b[i].mass
		py += b[i].vy * b[i].mass
		pz += b[i].vz * b[i].mass
	}
	b[0].vx = -px / b[0].mass
	b[0].vy = -py / b[0].mass
	b[0].vz = -pz / b[0].mass
}

func energy(b *[5]Body) float64 {
	e := 0.0
	for i := range b {
		body := b[i]
		e += 0.5 * body.mass * (body.vx*body.vx + body.vy*body.vy + body.vz*body.vz)
		for j := i + 1; j < len(b); j++ {
			dx, dy, dz := body.x-b[j].x, body.y-b[j].y, body.z-b[j].z
			e -= body.mass * b[j].mass / math.Sqrt(dx*dx+dy*dy+dz*dz)
		}
	}
	return e
}

func advance(b *[5]Body, dt float64) {
	for i := range b {
		for j := i + 1; j < len(b); j++ {
			dx, dy, dz := b[i].x-b[j].x, b[i].y-b[j].y, b[i].z-b[j].z
			d2 := dx*dx + dy*dy + dz*dz
			mag := dt / (d2 * math.Sqrt(d2))
			mi, mj := b[i].mass*mag, b[j].mass*mag
			b[i].vx -= dx * mj
			b[i].vy -= dy * mj
			b[i].vz -= dz * mj
			b[j].vx += dx * mi
			b[j].vy += dy * mi
			b[j].vz += dz * mi
		}
	}
	for i := range b {
		b[i].x += dt * b[i].vx
		b[i].y += dt * b[i].vy
		b[i].z += dt * b[i].vz
	}
}

func main() {
	bodies := system()
	offsetMomentum(&bodies)
	fmt.Printf("%.17g\n", energy(&bodies))
	for i := 0; i < 5_000_000; i++ {
		advance(&bodies, 0.01)
	}
	fmt.Printf("%.17g\n", energy(&bodies))
}
