import { useEffect, useMemo, useRef, useState } from 'react';
import {
  AmbientLight,
  BufferGeometry,
  Color,
  ConeGeometry,
  CylinderGeometry,
  DoubleSide,
  GridHelper,
  Line,
  LineBasicMaterial,
  Mesh,
  MeshBasicMaterial,
  Object3D,
  PerspectiveCamera,
  PlaneGeometry,
  Quaternion,
  Scene,
  TorusGeometry,
  Vector3,
  WebGLRenderer,
  ArrowHelper,
} from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { CSS2DObject, CSS2DRenderer } from 'three/addons/renderers/CSS2DRenderer.js';
import { useTheme } from '@/components/ThemeProvider';
import type { CoordinateSystem } from '@/domain/model';
import { arcEnd, arcPoints, rotate, toThree, tireScene, type SceneRole, type Vec3 } from '@/domain/tire-scene';

const ROLE_TOKENS: Record<SceneRole, string> = {
  axis: '--scene-axis',
  force: '--scene-force',
  moment: '--scene-moment',
  angle: '--scene-angle',
  velocity: '--scene-angle',
  spin: '--scene-angle',
};

const FALLBACK_COLOUR = '#888888';
const ARC_SEGMENTS = 48;

const vector = (v: Vec3): Vector3 => new Vector3(...toThree(v));

function token(name: string): string {
  const value = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  return value === '' ? FALLBACK_COLOUR : value;
}

function disposeAll(root: Object3D): void {
  root.traverse((object) => {
    if (object instanceof Mesh || object instanceof Line) {
      object.geometry.dispose();
      const materials: unknown = object.material;
      for (const material of Array.isArray(materials) ? materials : [materials]) {
        (material as { dispose(): void }).dispose();
      }
    }
  });
}

function label(text: string, id: string, position: Vector3, colour: string): CSS2DObject {
  const element = document.createElement('span');
  element.dataset.testid = `tire-scene-label-${id}`;
  element.textContent = text;
  element.style.fontSize = '11px';
  element.style.fontWeight = '500';
  element.style.whiteSpace = 'nowrap';
  element.style.color = colour;
  const object = new CSS2DObject(element);
  object.position.copy(position);
  return object;
}

function buildScene(system: CoordinateSystem): Scene {
  const description = tireScene(system);
  const colours = (role: SceneRole): string => token(ROLE_TOKENS[role]);
  const scene = new Scene();
  scene.add(new AmbientLight(0xffffff, 1));

  const ground = new Mesh(
    new PlaneGeometry(6, 6),
    new MeshBasicMaterial({ color: new Color(token('--scene-ground')), side: DoubleSide, transparent: true, opacity: 0.6 }),
  );
  ground.rotation.x = -Math.PI / 2;
  ground.position.y = -0.002;
  scene.add(ground);
  scene.add(new GridHelper(6, 12, new Color(token('--scene-tire')), new Color(token('--scene-ground'))));

  const { tire } = description;
  const axle = vector(rotate([0, 1, 0], [1, 0, 0], tire.camber));
  const tireColour = new Color(token('--scene-tire'));
  const body = new Mesh(
    new CylinderGeometry(tire.radius, tire.radius, tire.width, 48),
    new MeshBasicMaterial({ color: tireColour, transparent: true, opacity: 0.25 }),
  );
  body.quaternion.copy(new Quaternion().setFromUnitVectors(new Vector3(0, 1, 0), axle));
  body.position.copy(vector(tire.center));
  const tread = new Mesh(
    new TorusGeometry(tire.radius, 0.04, 12, 64),
    new MeshBasicMaterial({ color: tireColour }),
  );
  tread.quaternion.copy(new Quaternion().setFromUnitVectors(new Vector3(0, 0, 1), axle));
  tread.position.copy(vector(tire.center));
  scene.add(body, tread);

  for (const arrow of description.arrows) {
    const direction = vector(arrow.direction).normalize();
    const origin = vector(arrow.origin);
    scene.add(new ArrowHelper(direction, origin, arrow.length, new Color(colours(arrow.role)), 0.2, 0.1));
    scene.add(label(arrow.label, arrow.id, origin.clone().addScaledVector(direction, arrow.length * 1.08), colours(arrow.role)));
  }

  for (const arc of description.arcs) {
    const colour = colours(arc.role);
    const points = arcPoints(arc, ARC_SEGMENTS).map(vector);
    scene.add(new Line(new BufferGeometry().setFromPoints(points), new LineBasicMaterial({ color: new Color(colour) })));
    const end = arcEnd(arc);
    const head = new Mesh(new ConeGeometry(0.05, 0.15, 12), new MeshBasicMaterial({ color: new Color(colour) }));
    head.position.copy(vector(end.point));
    head.quaternion.copy(new Quaternion().setFromUnitVectors(new Vector3(0, 1, 0), vector(end.tangent).normalize()));
    scene.add(head);
    scene.add(label(arc.label, arc.id, points[ARC_SEGMENTS / 2] ?? points[0]!, colour));
  }
  return scene;
}

/** Orbitable 3D view of the axis conventions of a system (plain three.js, rendered on demand). */
export default function TireScene({ system }: { system: CoordinateSystem }): React.JSX.Element {
  const scene = useMemo(() => tireScene(system), [system]);
  const { theme } = useTheme();
  const mount = useRef<HTMLDivElement>(null);
  const [failed, setFailed] = useState(false);
  const view = useRef<{ position: Vector3; target: Vector3 } | null>(null);

  useEffect(() => {
    const host = mount.current;
    if (host === null) {
      return undefined;
    }
    let renderer: WebGLRenderer;
    try {
      renderer = new WebGLRenderer({ antialias: true, alpha: true });
      if (renderer.getContext() === null) {
        renderer.dispose();
        setFailed(true);
        return undefined;
      }
    } catch {
      setFailed(true);
      return undefined;
    }
    setFailed(false);

    const three = buildScene(system);
    const labels = new CSS2DRenderer();
    labels.domElement.style.position = 'absolute';
    labels.domElement.style.top = '0';
    labels.domElement.style.left = '0';
    labels.domElement.style.pointerEvents = 'none';
    host.append(renderer.domElement, labels.domElement);

    const camera = new PerspectiveCamera(40, 1, 0.1, 100);
    camera.position.copy(vector([4.5, 3.5, 2.8]));
    const target = vector([0, 0, 0.8]);
    camera.lookAt(target);
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.target.copy(target);
    if (view.current !== null) {
      camera.position.copy(view.current.position);
      controls.target.copy(view.current.target);
    }
    controls.update();

    const render = () => {
      renderer.render(three, camera);
      labels.render(three, camera);
    };
    const resize = () => {
      const width = Math.max(host.clientWidth, 1);
      const height = Math.max(host.clientHeight, 1);
      renderer.setSize(width, height);
      labels.setSize(width, height);
      camera.aspect = width / height;
      camera.updateProjectionMatrix();
      render();
    };
    controls.addEventListener('change', render);
    const observer = new ResizeObserver(resize);
    observer.observe(host);
    resize();

    return () => {
      view.current = { position: camera.position.clone(), target: controls.target.clone() };
      observer.disconnect();
      controls.removeEventListener('change', render);
      controls.dispose();
      disposeAll(three);
      renderer.forceContextLoss();
      renderer.dispose();
      renderer.domElement.remove();
      labels.domElement.remove();
    };
  }, [system, theme]);

  const captions = (
    <ul data-testid="tire-scene-captions" className="list-disc pl-4 text-xs text-muted-foreground">
      {scene.captions.map((caption) => (
        <li key={caption}>{caption}</li>
      ))}
    </ul>
  );
  return (
    <div>
      <div
        data-testid="tire-scene"
        data-system={system}
        role="img"
        aria-label={`${scene.title}: ${scene.captions.join('; ')}`}
        className="relative h-64 w-full"
      >
        <div ref={mount} className="absolute inset-0 overflow-hidden" />
        {failed && (
          <div data-testid="tire-scene-fallback" className="absolute inset-0 overflow-auto bg-card">
            {captions}
          </div>
        )}
      </div>
      {!failed && captions}
    </div>
  );
}
