// context-menu.js — Right-click context menu for the beatmap editor.
// Pure DOM component: builds HTML menus dynamically, calls state mutations on click.

import {
  state, getActiveBeats, addBeat, removeBeat, updateBeat, pushUndo
} from './state.js';

import {
  SHAPES, LANES, KINDS_WITH_SHAPE,
  KIND_LABELS, SHAPE_LABELS, LANE_LABELS,
  EDITOR_OBSTACLE_KINDS, isEditorObstacleKind
} from './constants.js';

let container = null;
const AUTHORING_KINDS = EDITOR_OBSTACLE_KINDS.filter((kind) => kind !== 'combo_gate');

// ── Helpers ─────────────────────────────────────────

function createItem(label, opts = {}) {
  const div = document.createElement('div');
  div.classList.add('ctx-item');
  if (opts.checked) div.classList.add('checked');

  const textNode = document.createTextNode(label);
  div.appendChild(textNode);

  if (opts.hasSubmenu) {
    const arrow = document.createElement('span');
    arrow.classList.add('submenu-arrow');
    arrow.textContent = '▸';
    div.appendChild(arrow);
  }

  if (opts.onClick) {
    div.addEventListener('click', (e) => {
      e.stopPropagation();
      opts.onClick();
    });
  }

  return div;
}

function createSeparator() {
  const div = document.createElement('div');
  div.classList.add('ctx-separator');
  return div;
}

function createSubmenu(items) {
  const sub = document.createElement('div');
  sub.classList.add('ctx-submenu');
  for (const item of items) sub.appendChild(item);
  return sub;
}

// ── Existing-obstacle menu ──────────────────────────

function buildObstacleMenu(context) {
  const beats = getActiveBeats();
  const obstacle = beats[context.obstacleIndex];

  // Change Kind ►
  const kindItem = createItem('Change Kind', { hasSubmenu: true });
  const kindSubs = AUTHORING_KINDS.map((kind) =>
    createItem(KIND_LABELS[kind], {
      checked: obstacle.kind === kind,
      onClick() {
        if (!isEditorObstacleKind(kind)) return;
        pushUndo('Change obstacle kind');
        updateBeat(context.obstacleIndex, { kind });
        hide();
      },
    })
  );
  kindItem.appendChild(createSubmenu(kindSubs));
  container.appendChild(kindItem);
  container.appendChild(createSeparator());

  // Change Shape ► (only for kinds that have shapes)
  if (KINDS_WITH_SHAPE.includes(obstacle.kind)) {
    const shapeItem = createItem('Change Shape', { hasSubmenu: true });
    const shapeSubs = SHAPES.map((shape) =>
      createItem(SHAPE_LABELS[shape], {
        checked: obstacle.shape === shape,
        onClick() {
          pushUndo('Change obstacle shape');
          updateBeat(context.obstacleIndex, { shape });
          hide();
        },
      })
    );
    shapeItem.appendChild(createSubmenu(shapeSubs));
    container.appendChild(shapeItem);
    container.appendChild(createSeparator());
  }

  // Change Lane ►
  const laneItem = createItem('Change Lane', { hasSubmenu: true });
  const laneSubs = LANES.map((lane) =>
    createItem(LANE_LABELS[lane], {
      checked: obstacle.lane === lane,
      onClick() {
        pushUndo('Change obstacle lane');
        updateBeat(context.obstacleIndex, { lane });
        hide();
      },
    })
  );
  laneItem.appendChild(createSubmenu(laneSubs));
  container.appendChild(laneItem);
  container.appendChild(createSeparator());

  // Duplicate
  container.appendChild(
    createItem('Duplicate', {
      onClick() {
        const original = getActiveBeats()[context.obstacleIndex];
        pushUndo('Duplicate obstacle');
        addBeat({ ...original, beat: original.beat + 1 });
        hide();
      },
    })
  );

  // Delete
  container.appendChild(
    createItem('Delete', {
      onClick() {
        pushUndo('Delete obstacle');
        removeBeat(context.obstacleIndex);
        hide();
      },
    })
  );
}

// ── Empty-space menu ────────────────────────────────

function buildEmptyMenu(context) {
  for (const kind of AUTHORING_KINDS) {
    const hasShape = KINDS_WITH_SHAPE.includes(kind);
    const label = `Place ${KIND_LABELS[kind]}`;

    if (hasShape) {
      const item = createItem(label, { hasSubmenu: true });
      const subs = SHAPES.map((shape) =>
        createItem(SHAPE_LABELS[shape], {
          onClick() {
            if (!isEditorObstacleKind(kind)) return;
            pushUndo('Place obstacle');
            addBeat({ beat: context.beat, kind, shape, lane: context.lane });
            hide();
          },
        })
      );
      item.appendChild(createSubmenu(subs));
      container.appendChild(item);
    } else {
      container.appendChild(
        createItem(label, {
          onClick() {
            if (!isEditorObstacleKind(kind)) return;
            pushUndo('Place obstacle');
            addBeat({ beat: context.beat, kind, lane: context.lane });
            hide();
          },
        })
      );
    }

    // Separator after first item (ShapeGate) to match spec layout
    if (kind === 'shape_gate') {
      container.appendChild(createSeparator());
    }
  }
}

// ── Viewport clamping ───────────────────────────────

function clampPosition(x, y) {
  const rect = container.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;
  const vw = window.innerWidth;
  const vh = window.innerHeight;

  if (x + w > vw) x = x - w;
  if (y + h > vh) y = y - h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  container.style.left = `${x}px`;
  container.style.top = `${y}px`;
}

// ── Public API ──────────────────────────────────────

export function init(el) {
  container = el;

  document.addEventListener('click', (e) => {
    if (isVisible() && !container.contains(e.target)) {
      hide();
    }
  });

  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && isVisible()) {
      hide();
    }
  });
}

export function show(x, y, context) {
  container.innerHTML = '';

  if (context.obstacleIndex !== null && context.obstacleIndex !== undefined) {
    buildObstacleMenu(context);
  } else {
    buildEmptyMenu(context);
  }

  // Position offscreen first so we can measure true size
  container.style.left = '-9999px';
  container.style.top = '-9999px';
  container.classList.add('visible');

  // Defer clamping to next frame so layout is computed
  requestAnimationFrame(() => clampPosition(x, y));
}

export function hide() {
  container.classList.remove('visible');
  container.innerHTML = '';
}

export function isVisible() {
  return container !== null && container.classList.contains('visible');
}
