#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Análisis de avalanchas en silos
Definición:
- Una avalancha comienza en el primer cambio de NoPTotal tras inactividad
- Termina cuando pasan >= umbral_s (p.ej. 5 s) sin cambios en NoPTotal
- Tamaño de la avalancha = suma de aumentos positivos de NoPOriginalTotal dentro del bloque
Salidas:
- CSV crudos por archivo (tamaños)
- CSV agregados por (chi, outlet)
- (Opcional) tablas gnuplot (bin_center, density)
- (Opcional) plots matplotlib normalizados (densidad), con opción logY
"""

import argparse
import os
import re
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import numpy as np
import pandas as pd

# Opcional para plots
try:
    import matplotlib.pyplot as plt
    HAVE_PLT = True
except Exception:
    HAVE_PLT = False


# -----------------------------
# Lectura robusta de flow_data
# -----------------------------

def leer_flow_csv(ruta: Path) -> pd.DataFrame:
    """
    Lee flow_data.csv de forma robusta (coma/; /tab/espacios).
    Devuelve columnas normalizadas: Time, NoPTotal, NoPOriginalTotal (float).
    """
    ruta = Path(ruta)
    if not ruta.exists():
        raise FileNotFoundError(f"No existe el archivo: {ruta}")

    last_err = None
    for sep in [",", ";", "\t", r"\s+"]:
        try:
            df = pd.read_csv(ruta, sep=sep, engine="python")
            break
        except Exception as e:
            last_err = e
            df = None
    if df is None:
        raise RuntimeError(f"No se pudo leer {ruta}: {last_err}")

    # Normalizar nombres de columnas (case-insensitive, tolerante a espacios)
    def norm(s: str) -> str:
        return s.strip().lower().replace(" ", "")

    cols = {norm(c): c for c in df.columns}
    req = {"time": None, "noptotal": None, "noporiginaltotal": None}
    for k in list(req.keys()):
        if k in cols:
            req[k] = cols[k]
        else:
            # fallback: buscar aproximados
            for c in df.columns:
                if norm(c) == k:
                    req[k] = c
                    break

    missing = [k for k, v in req.items() if v is None]
    if missing:
        raise ValueError(f"{ruta} no contiene columnas requeridas {missing}. Encontradas: {list(df.columns)}")

    out = df[[req["time"], req["noptotal"], req["noporiginaltotal"]]].copy()
    out.columns = ["Time", "NoPTotal", "NoPOriginalTotal"]
    out["Time"] = pd.to_numeric(out["Time"], errors="coerce")
    out["NoPTotal"] = pd.to_numeric(out["NoPTotal"], errors="coerce")
    out["NoPOriginalTotal"] = pd.to_numeric(out["NoPOriginalTotal"], errors="coerce")
    out = out.dropna(subset=["Time", "NoPTotal", "NoPOriginalTotal"]).sort_values("Time").reset_index(drop=True)
    return out


# -----------------------------
# Detección de avalanchas
# -----------------------------

def detectar_avalanchas_con_umbral_tiempo(
    df: pd.DataFrame,
    umbral_s: float = 5.0,
    min_size: int = 1,
    cerrar_ultima_al_final: bool = True,
) -> List[int]:
    """
    Agrupa cambios en NoPTotal en bloques separados por gaps >= umbral_s.
    Tamaño de avalancha = suma de aumentos positivos de NoPOriginalTotal dentro del bloque.
    Devuelve lista de tamaños (enteros).
    """
    t = df["Time"].to_numpy()
    nop = df["NoPTotal"].to_numpy()
    nop_orig = df["NoPOriginalTotal"].to_numpy()

    dN   = np.diff(nop, prepend=nop[0])           # cambios de NoPTotal
    dOrg = np.diff(nop_orig, prepend=nop_orig[0])  # cambios de NoPOriginalTotal

    idx_cambios = np.flatnonzero(dN != 0)
    if idx_cambios.size == 0:
        return []

    tamanios: List[int] = []
    start_idx = idx_cambios[0]
    last_idx  = idx_cambios[0]

    for k in range(1, len(idx_cambios)):
        curr_idx = idx_cambios[k]
        gap = t[curr_idx] - t[last_idx]
        if gap >= umbral_s:
            # cerrar bloque en last_idx
            incs = dOrg[start_idx:last_idx+1]
            size = int(np.maximum(incs, 0).sum())
            if size >= min_size:
                tamanios.append(size)
            start_idx = curr_idx
        last_idx = curr_idx

    # Cerrar último bloque (opcional)
    if cerrar_ultima_al_final:
        incs = dOrg[start_idx:last_idx+1]
        size = int(np.maximum(incs, 0).sum())
        if size >= min_size:
            tamanios.append(size)

    return tamanios


# -----------------------------
# Extraer chi y outlet desde path
# -----------------------------

_RE_CHI = re.compile(r"chi(\d+(?:\.\d+)?)", re.IGNORECASE)
_RE_OUT = re.compile(r"outlet(\d+(?:\.\d+)?)", re.IGNORECASE)

def extraer_parametros_desde_path(p: Path) -> Tuple[Optional[float], Optional[float]]:
    s = str(p)
    m1 = _RE_CHI.search(s)
    m2 = _RE_OUT.search(s)
    chi = float(m1.group(1)) if m1 else None
    outlet = float(m2.group(1)) if m2 else None
    return chi, outlet


# -----------------------------
# Guardado de resultados
# -----------------------------

def guardar_avalanchas_csv(tamanios: List[int], destino: Path) -> Path:
    destino.parent.mkdir(parents=True, exist_ok=True)
    pd.DataFrame({"tamaño_avalancha": tamanios}).to_csv(destino, index=False)
    print(f"[OK] CSV guardado: {destino} (n={len(tamanios)})")
    return destino


def escribir_histograma_gnuplot(tamanios: List[int], binw: int, destino: Path):
    """
    Escribe tabla (bin_center, density) normalizada a densidad: freq/(N*binw)
    """
    if not tamanios:
        return
    N = len(tamanios)
    arr = np.asarray(tamanios, dtype=float)
    x_min = 0.0
    x_max = arr.max()
    num_bins = int(np.floor((x_max - x_min) / binw)) + 1
    centers = x_min + (np.arange(num_bins) + 0.5) * binw

    idx = np.floor((arr - x_min) / binw).astype(int)
    idx = idx[(idx >= 0) & (idx < num_bins)]
    counts = np.bincount(idx, minlength=num_bins)
    density = counts / (N * float(binw))

    destino.parent.mkdir(parents=True, exist_ok=True)
    pd.DataFrame({"bin_center": centers, "density": density}).to_csv(destino, index=False)
    print(f"[OK] Tabla gnuplot: {destino} (binw={binw}, bins={num_bins})")


def plot_hist_densidad(
    tamanios: List[int],
    titulo: str,
    destino_png: Path,
    bins: Optional[int] = None,
    binw: Optional[int] = None,
    logy: bool = True,
):
    if not HAVE_PLT or not tamanios:
        return
    arr = np.asarray(tamanios, dtype=float)
    destino_png.parent.mkdir(parents=True, exist_ok=True)
    plt.figure(figsize=(9, 6))
    # Definir bins por cantidad o por ancho
    if binw is not None:
        xmax = arr.max()
        edges = np.arange(0, xmax + binw, binw)
    elif bins is not None:
        edges = bins
    else:
        # Freedman-Diaconis como fallback
        q75, q25 = np.percentile(arr, [75, 25])
        iqr = max(q75 - q25, 1.0)
        binw_fd = max(2 * iqr / (len(arr) ** (1.0 / 3.0)), 1.0)
        edges = np.arange(0, arr.max() + binw_fd, binw_fd)

    plt.hist(arr, bins=edges, density=True, alpha=0.7, edgecolor="black")
    plt.xlabel("Tamaño de avalancha s")
    plt.ylabel("Densidad de probabilidad")
    if logy:
        plt.yscale("log")
    plt.title(titulo)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(destino_png, dpi=200)
    plt.close()
    print(f"[OK] Plot guardado: {destino_png}")


def plot_multi_grupos(
    datos_por_grupo: Dict[Tuple[Optional[float], Optional[float]], List[int]],
    destino_png: Path,
    binw: int = 200,
    logy: bool = True,
):
    if not HAVE_PLT or not datos_por_grupo:
        return
    destino_png.parent.mkdir(parents=True, exist_ok=True)
    plt.figure(figsize=(10, 7))
    for (chi, outlet), datos in sorted(datos_por_grupo.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        if not datos:
            continue
        arr = np.asarray(datos, dtype=float)
        xmax = arr.max()
        edges = np.arange(0, xmax + binw, binw)
        # hist normalizado (density=True), step para que se superpongan bien
        h, be = np.histogram(arr, bins=edges, density=True)
        centers = 0.5 * (be[:-1] + be[1:])
        label = f"χ={chi}" + (f", outlet={outlet}" if outlet is not None else "")
        plt.plot(centers, h, lw=2, label=label)
    if logy:
        plt.yscale("log")
    plt.xlabel("Tamaño de avalancha s")
    plt.ylabel("Densidad de probabilidad")
    plt.title("Histogramas normalizados por grupo")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(destino_png, dpi=220)
    plt.close()
    print(f"[OK] Plot multi guardado: {destino_png}")


# -----------------------------
# Procesamiento
# -----------------------------

def procesar_archivo(ruta_csv: Path, umbral_s: float, min_size: int) -> List[int]:
    df = leer_flow_csv(ruta_csv)
    sizes = detectar_avalanchas_con_umbral_tiempo(df, umbral_s=umbral_s, min_size=min_size)
    return sizes


def procesar_arbol(
    carpeta_raiz: Path,
    carpeta_salida: Path,
    umbral_s: float = 5.0,
    min_size: int = 1,
    exportar_gnuplot: bool = False,
    binw: int = 200,
    hacer_plots: bool = False,
    logy: bool = True,
    patron_archivo: str = "flow_data.csv",
) -> Dict[Tuple[Optional[float], Optional[float]], List[int]]:
    carpeta_raiz = Path(carpeta_raiz)
    carpeta_salida = Path(carpeta_salida)
    resultados: Dict[Tuple[Optional[float], Optional[float]], List[int]] = {}

    archivos = list(carpeta_raiz.rglob(patron_archivo))
    if not archivos:
        print(f"[WARN] No se encontraron '{patron_archivo}' bajo {carpeta_raiz}")
        return resultados

    print(f"[INFO] Encontrados {len(archivos)} archivos '{patron_archivo}'")

    for ruta in sorted(archivos):
        chi, outlet = extraer_parametros_desde_path(ruta)
        clave = (chi, outlet)
        print("-" * 70)
        print(f"[PROC] {ruta}")
        print(f"       chi={chi}  outlet={outlet}")

        try:
            tamanios = procesar_archivo(ruta, umbral_s=umbral_s, min_size=min_size)
        except Exception as e:
            print(f"[ERROR] {ruta}: {e}")
            continue

        resultados.setdefault(clave, []).extend(tamanios)

        # Guardar CSV crudo por archivo
        out_crudos = carpeta_salida / "crudos"
        hash_corto = abs(hash(str(ruta))) % 10_000_000
        base_name = f"datos_avalanchas{f'_chi{chi}' if chi is not None else ''}{f'_outlet{outlet}' if outlet is not None else ''}_{hash_corto}.csv"
        guardar_avalanchas_csv(tamanios, out_crudos / base_name)

        # Tabla gnuplot por archivo (opcional)
        if exportar_gnuplot:
            out_gp = carpeta_salida / "crudos"
            gp_name = base_name.replace(".csv", f"_gp_bin{binw}.csv")
            escribir_histograma_gnuplot(tamanios, binw=binw, destino=out_gp / gp_name)

        # Plot individual por archivo (opcional)
        if hacer_plots and HAVE_PLT and len(tamanios) > 0:
            titulo = f"Hist. (densidad) — chi={chi}, outlet={outlet}"
            plot_hist_densidad(
                tamanios,
                titulo=titulo,
                destino_png=(carpeta_salida / "plots" / f"hist_{hash_corto}.png"),
                binw=binw,
                logy=logy,
            )

    # Guardar agregados por grupo
    print("\n" + "=" * 70)
    print("RESUMEN POR GRUPO (chi, outlet)")
    print("=" * 70)

    out_grupos = carpeta_salida / "grupos"
    out_grupos.mkdir(parents=True, exist_ok=True)

    for (chi, outlet), datos in resultados.items():
        n = len(datos)
        media = np.mean(datos) if n else float("nan")
        print(f"chi={chi}  outlet={outlet}  n={n}  <s>={media:.2f}")

        nombre = f"datos_avalanchas{f'_chi{chi}' if chi is not None else ''}{f'_outlet{outlet}' if outlet is not None else ''}.csv"
        guardar_avalanchas_csv(datos, out_grupos / nombre)

        if exportar_gnuplot:
            gp_nombre = f"hist_gnuplot{f'_chi{chi}' if chi is not None else ''}{f'_outlet{outlet}' if outlet is not None else ''}_bin{binw}.csv"
            escribir_histograma_gnuplot(datos, binw=binw, destino=out_grupos / gp_nombre)

        if hacer_plots and HAVE_PLT and len(datos) > 0:
            titulo = f"Hist. (densidad) agregado — chi={chi}, outlet={outlet}"
            plot_hist_densidad(
                datos,
                titulo=titulo,
                destino_png=(carpeta_salida / "plots" / f"hist_chi{chi}_out{outlet}.png"),
                binw=binw,
                logy=logy,
            )

    # Plot comparativo multi-grupo
    if hacer_plots and HAVE_PLT:
        plot_multi_grupos(
            datos_por_grupo=resultados,
            destino_png=(carpeta_salida / "plots" / "hist_multi_grupos.png"),
            binw=binw,
            logy=logy,
        )

    return resultados


# -----------------------------
# CLI
# -----------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Análisis de avalanchas (bloques separados por >= umbral_s sin cambios en NoPTotal).")
    p.add_argument("--raiz", type=Path, default=Path("/home/jmontero/Box2D/Box2D-Silo/simulations_discos"),
                   help="Carpeta raíz que contiene subcarpetas con flow_data.csv")
    p.add_argument("--salida", type=Path, default=Path("resultados_avalanchas"),
                   help="Carpeta de salida para CSVs/tablas/plots")
    p.add_argument("--umbral", type=float, default=5.0, help="Umbral de inactividad (s) para cerrar avalancha")
    p.add_argument("--min-size", type=int, default=1, help="Tamaño mínimo de avalancha a considerar")
    p.add_argument("--gnuplot", action="store_true", help="Exportar tablas gnuplot (bin_center, density)")
    p.add_argument("--binw", type=int, default=200, help="Ancho de bin para tablas gnuplot y plots")
    p.add_argument("--plots", action="store_true", help="Generar plots matplotlib (si está disponible)")
    p.add_argument("--no-logy", action="store_true", help="No usar escala logarítmica en Y en los plots")
    p.add_argument("--patron-archivo", type=str, default="flow_data.csv", help="Patrón de archivo a buscar (rglob)")
    return p

def main():
    args = build_parser().parse_args()
    logy = not args.no_logy
    procesar_arbol(
        carpeta_raiz=args.raiz,
        carpeta_salida=args.salida,
        umbral_s=args.umbral,
        min_size=args.min_size,
        exportar_gnuplot=args.gnuplot,
        binw=args.binw,
        hacer_plots=args.plots,
        logy=logy,
        patron_archivo=args.patron_archivo,
    )

if __name__ == "__main__":
    main()
