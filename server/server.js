const fs = require("fs");
const http = require("http");
const path = require("path");
const { URL } = require("url");

const PORT = Number(process.env.MINI_GIT_SERVER_PORT || "4000");
const DATA_ROOT = path.resolve(process.env.MINI_GIT_SERVER_DATA || path.join(__dirname, "data"));

function ensureDir(dirPath) {
  fs.mkdirSync(dirPath, { recursive: true });
}

function readRequestBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    req.on("data", (chunk) => chunks.push(chunk));
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}

function sendText(res, status, body) {
  const payload = Buffer.from(body, "utf8");
  res.writeHead(status, {
    "Content-Type": "text/plain; charset=utf-8",
    "Content-Length": payload.length,
  });
  res.end(payload);
}

function validateRepoName(repoName) {
  if (!/^[A-Za-z0-9._-]+$/.test(repoName)) {
    throw new Error("Invalid repo name");
  }
}

function repoPaths(repoName) {
  validateRepoName(repoName);
  const repoRoot = path.join(DATA_ROOT, repoName);
  return {
    repoRoot,
    currentBranch: path.join(repoRoot, "CURRENT_BRANCH"),
    stash: path.join(repoRoot, "stash"),
    refsRoot: path.join(repoRoot, "refs", "heads"),
    objectsRoot: path.join(repoRoot, "objects"),
  };
}

function parseSnapshot(payload) {
  const lines = payload.split(/\r?\n/);
  if (lines[0] !== "MINIGIT_REMOTE_V1") {
    throw new Error("Invalid snapshot header");
  }

  const snapshot = {
    current_branch: "",
    stash: "",
    refs: [],
    objects: [],
  };

  for (let index = 1; index < lines.length; index += 1) {
    const line = lines[index];
    if (!line) {
      continue;
    }
    if (line === "END") {
      return snapshot;
    }

    const firstSpace = line.indexOf(" ");
    const kind = firstSpace === -1 ? line : line.slice(0, firstSpace);
    const rest = firstSpace === -1 ? "" : line.slice(firstSpace + 1);

    if (kind === "CURRENT_BRANCH") {
      snapshot.current_branch = rest === "-" ? "" : rest;
    } else if (kind === "STASH") {
      snapshot.stash = rest === "-" ? "" : rest;
    } else if (kind === "REF") {
      const split = rest.indexOf(" ");
      if (split === -1) {
        throw new Error("Invalid REF line");
      }
      const name = rest.slice(0, split);
      const value = rest.slice(split + 1);
      snapshot.refs.push([name, value === "-" ? "" : value]);
    } else if (kind === "OBJECT") {
      const split = rest.indexOf(" ");
      if (split === -1) {
        throw new Error("Invalid OBJECT line");
      }
      const objectPath = rest.slice(0, split);
      const hexContent = rest.slice(split + 1);
      snapshot.objects.push([objectPath, hexContent]);
    } else {
      throw new Error(`Unknown line type: ${kind}`);
    }
  }

  throw new Error("Snapshot missing END");
}

function serializeSnapshot(snapshot) {
  const lines = [
    "MINIGIT_REMOTE_V1",
    `CURRENT_BRANCH ${snapshot.current_branch || "-"}`,
    `STASH ${snapshot.stash || "-"}`,
  ];

  snapshot.refs
    .slice()
    .sort((left, right) => left[0].localeCompare(right[0]))
    .forEach(([name, value]) => {
      lines.push(`REF ${name} ${value || "-"}`);
    });

  snapshot.objects
    .slice()
    .sort((left, right) => left[0].localeCompare(right[0]))
    .forEach(([objectPath, hexContent]) => {
      lines.push(`OBJECT ${objectPath} ${hexContent}`);
    });

  lines.push("END");
  return `${lines.join("\n")}\n`;
}

function saveSnapshot(repoName, snapshot) {
  const paths = repoPaths(repoName);
  ensureDir(paths.refsRoot);
  ensureDir(paths.objectsRoot);

  fs.writeFileSync(paths.currentBranch, snapshot.current_branch, "utf8");
  if (snapshot.stash) {
    fs.writeFileSync(paths.stash, snapshot.stash, "utf8");
  } else if (fs.existsSync(paths.stash)) {
    fs.unlinkSync(paths.stash);
  }

  snapshot.refs.forEach(([name, value]) => {
    fs.writeFileSync(path.join(paths.refsRoot, name), value, "utf8");
  });

  snapshot.objects.forEach(([objectPath, hexContent]) => {
    const filePath = path.join(paths.objectsRoot, objectPath);
    ensureDir(path.dirname(filePath));
    fs.writeFileSync(filePath, Buffer.from(hexContent, "hex"));
  });
}

function loadSnapshot(repoName) {
  const paths = repoPaths(repoName);
  if (!fs.existsSync(paths.repoRoot)) {
    return null;
  }

  const snapshot = {
    current_branch: fs.existsSync(paths.currentBranch) ? fs.readFileSync(paths.currentBranch, "utf8").trim() : "",
    stash: fs.existsSync(paths.stash) ? fs.readFileSync(paths.stash, "utf8").trim() : "",
    refs: [],
    objects: [],
  };

  if (fs.existsSync(paths.refsRoot)) {
    for (const name of fs.readdirSync(paths.refsRoot)) {
      const refPath = path.join(paths.refsRoot, name);
      if (fs.statSync(refPath).isFile()) {
        snapshot.refs.push([name, fs.readFileSync(refPath, "utf8").trim()]);
      }
    }
  }

  if (fs.existsSync(paths.objectsRoot)) {
    const walk = (dirPath) => {
      for (const entry of fs.readdirSync(dirPath, { withFileTypes: true })) {
        const fullPath = path.join(dirPath, entry.name);
        if (entry.isDirectory()) {
          walk(fullPath);
        } else if (entry.isFile()) {
          snapshot.objects.push([
            path.relative(paths.objectsRoot, fullPath).split(path.sep).join("/"),
            fs.readFileSync(fullPath).toString("hex"),
          ]);
        }
      }
    };
    walk(paths.objectsRoot);
  }

  return snapshot;
}

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);
    const parts = url.pathname.split("/").filter(Boolean);

    if (req.method === "GET" && url.pathname === "/health") {
      sendText(res, 200, "ok\n");
      return;
    }

    if (parts.length === 4 && parts[0] === "api" && parts[1] === "repos") {
      const repoName = decodeURIComponent(parts[2]);
      const action = parts[3];

      if (req.method === "POST" && action === "push") {
        const snapshot = parseSnapshot(await readRequestBody(req));
        saveSnapshot(repoName, snapshot);
        sendText(res, 200, "OK\n");
        return;
      }

      if (req.method === "GET" && action === "pull") {
        const snapshot = loadSnapshot(repoName);
        if (!snapshot) {
          sendText(res, 404, "Repository not found\n");
          return;
        }
        sendText(res, 200, serializeSnapshot(snapshot));
        return;
      }
    }

    sendText(res, 404, "Not found\n");
  } catch (error) {
    sendText(res, 400, `${error.message}\n`);
  }
});

ensureDir(DATA_ROOT);
server.listen(PORT, () => {
  process.stdout.write(`Mini Git server listening on ${PORT}\n`);
});
