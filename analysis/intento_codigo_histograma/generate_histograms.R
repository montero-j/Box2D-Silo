#!/usr/bin/env Rscript

# Script para generar histogramas de tamaños de avalancha - USANDO SOLO BASE R

# Encontrar todos los archivos de datos
data_files <- list.files(pattern = "avalanche_sizes_.*_chi.*\\.csv")

# Crear directorio para gráficos si no existe
if (!dir.exists("histograms")) {
  dir.create("histograms")
}

# Data frame para estadísticas
all_stats <- data.frame()

for (file in data_files) {
  # Extraer información del nombre del archivo
  file_parts <- strsplit(file, "_")[[1]]
  particle_type <- file_parts[3]
  chi_value <- gsub("chi", "", file_parts[4])
  chi_value <- gsub("\\.csv", "", chi_value)
  chi_value <- gsub("_", ".", chi_value)  # Convertir 1_0 a 1.0
  
  # Leer datos
  data <- read.csv(file)
  sizes <- data$Avalanche_Size
  
  # Calcular estadísticas
  stats <- data.frame(
    Chi = as.numeric(chi_value),
    Particle_Type = particle_type,
    Mean = mean(sizes),
    Median = median(sizes),
    SD = sd(sizes),
    Min = min(sizes),
    Max = max(sizes),
    Total_Avalanches = length(sizes),
    Zero_Size_Count = sum(sizes == 0),
    Zero_Size_Percent = round(sum(sizes == 0) / length(sizes) * 100, 2)
  )
  
  all_stats <- rbind(all_stats, stats)
  
  # Crear histograma con R base
  png_filename <- paste0("histograms/histogram_", gsub("\\.csv", "", file), ".png")
  png(png_filename, width = 1000, height = 600, res = 150)
  
  # Configurar parámetros gráficos
  par(mar = c(5, 5, 4, 2) + 0.1)
  
  # Crear histograma
  hist_data <- hist(sizes, 
                   breaks = 30,
                   main = paste("Distribución de Tamaños de Avalancha\n", 
                              ifelse(particle_type == "discs", "Discos", 
                                     paste("Polígonos", gsub("poly", "", particle_type))),
                              "- χ =", chi_value),
                   xlab = "Tamaño de Avalancha (partículas originales)",
                   ylab = "Frecuencia",
                   col = "lightblue",
                   border = "black",
                   xlim = c(0, max(sizes) * 1.1),
                   cex.main = 1.2,
                   cex.lab = 1.1)
  
  # Añadir línea de media
  abline(v = mean(sizes), col = "red", lwd = 2, lty = 2)
  
  # Añadir línea de mediana
  abline(v = median(sizes), col = "blue", lwd = 2, lty = 2)
  
  # Añadir leyenda
  legend("topright", 
         legend = c(paste("Media =", round(mean(sizes), 2)),
                   paste("Mediana =", round(median(sizes), 2)),
                   paste("Total avalanchas =", length(sizes)),
                   paste("Avalanchas tamaño 0 =", sum(sizes == 0), 
                         paste0("(", round(sum(sizes == 0) / length(sizes) * 100, "%)"))),
         bty = "n",
         cex = 0.8)
  
  dev.off()
  
  # También crear gráfico de densidad
  png_filename_density <- paste0("histograms/density_", gsub("\\.csv", "", file), ".png")
  png(png_filename_density, width = 1000, height = 600, res = 150)
  
  par(mar = c(5, 5, 4, 2) + 0.1)
  
  # Gráfico de densidad
  density_data <- density(sizes)
  plot(density_data,
       main = paste("Densidad de Tamaños de Avalancha\n", 
                   ifelse(particle_type == "discs", "Discos", 
                          paste("Polígonos", gsub("poly", "", particle_type))),
                   "- χ =", chi_value),
       xlab = "Tamaño de Avalancha (partículas originales)",
       ylab = "Densidad",
       col = "darkblue",
       lwd = 2,
       cex.main = 1.2,
       cex.lab = 1.1)
  
  # Añadir área sombreada
  polygon(density_data, col = rgb(0, 0, 1, 0.3), border = NA)
  
  # Añadir líneas de media y mediana
  abline(v = mean(sizes), col = "red", lwd = 2, lty = 2)
  abline(v = median(sizes), col = "blue", lwd = 2, lty = 2)
  
  legend("topright", 
         legend = c("Densidad", 
                   paste("Media =", round(mean(sizes), 2)),
                   paste("Mediana =", round(median(sizes), 2))),
         col = c("darkblue", "red", "blue"),
         lty = c(1, 2, 2),
         lwd = c(2, 2, 2),
         bty = "n")
  
  dev.off()
  
  cat("Procesado:", file, "- Gráficos guardados en carpeta 'histograms/'\n")
}

# Ordenar estadísticas por Chi y tipo de partícula
all_stats <- all_stats[order(all_stats$Particle_Type, all_stats$Chi), ]

# Guardar estadísticas resumidas
write.csv(all_stats, "avalanche_statistics_summary.csv", row.names = FALSE)

# Crear un gráfico comparativo de medias por Chi
if (nrow(all_stats) > 0) {
  png("histograms/comparison_means_by_chi.png", width = 1200, height = 800, res = 150)
  
  # Separar por tipo de partícula
  particle_types <- unique(all_stats$Particle_Type)
  colors <- c("red", "blue", "green", "orange", "purple")
  
  par(mar = c(5, 5, 4, 8) + 0.1)
  
  plot(0, 0, type = "n",
       xlim = range(all_stats$Chi),
       ylim = c(0, max(all_stats$Mean) * 1.2),
       main = "Tamaño Medio de Avalancha vs Concentración χ",
       xlab = "Concentración χ",
       ylab = "Tamaño Medio de Avalancha",
       cex.main = 1.3,
       cex.lab = 1.1)
  
  # Añadir puntos y líneas para cada tipo de partícula
  for (i in 1:length(particle_types)) {
    type_data <- all_stats[all_stats$Particle_Type == particle_types[i], ]
    lines(type_data$Chi, type_data$Mean, 
          type = "b", 
          col = colors[i], 
          lwd = 2, 
          pch = 19,
          cex = 1.2)
  }
  
  # Añadir leyenda fuera del gráfico
  legend(par("usr")[2] * 1.01, par("usr")[4],
         legend = ifelse(particle_types == "discs", "Discos", 
                        paste("Polígonos", gsub("poly", "", particle_types))),
         col = colors[1:length(particle_types)],
         lwd = 2,
         pch = 19,
         bty = "n",
         xpd = TRUE)
  
  dev.off()
  cat("✅ Gráfico comparativo generado: histograms/comparison_means_by_chi.png\n")
}

cat("✅ Proceso completado!\n")
cat("✅ Estadísticas guardadas en: avalanche_statistics_summary.csv\n")
cat("✅ Todos los gráficos guardados en la carpeta: histograms/\n")
cat("\nResumen de archivos generados:\n")
cat("- avalanche_sizes_*.csv: Datos crudos de tamaños de avalancha\n")
cat("- avalanche_statistics_summary.csv: Estadísticas descriptivas\n")
cat("- histograms/histogram_*.png: Histogramas\n")
cat("- histograms/density_*.png: Gráficos de densidad\n")
cat("- histograms/comparison_means_by_chi.png: Comparación entre concentraciones\n")
