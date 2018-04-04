library(ggplot2)
library(reshape)

theme_Publication <- function(use_major_gridlines = TRUE, use_minor_gridlines = FALSE, base_size=14, base_family="helvetica") {
      library(grid)
      library(ggthemes)

      major_elem <- element_blank()
      if (use_major_gridlines) {
      	major_elem <- element_line(colour="#f0f0f0")
      }

      minor_elem <- element_blank()
      if (use_minor_gridlines) {
      	minor_elem <- element_line(colour="#f0f0f0")
      }

      (theme_foundation(base_size=base_size, base_family=base_family)
       + theme(plot.title = element_text(face = "bold",
                                         size = rel(1.2), hjust = 0.5),
               text = element_text(),
               panel.background = element_rect(colour = NA),
               plot.background = element_rect(colour = NA),
               panel.border = element_rect(colour = NA),
               axis.title = element_text(face = "bold",size = rel(1)),
               axis.title.y = element_text(angle=90,vjust =2),
               axis.title.x = element_text(vjust = -0.2),
               axis.text = element_text(), 
               axis.line = element_line(colour="black"),
               axis.ticks = element_line(),
               panel.grid.major = major_elem,
               panel.grid.minor = minor_elem,
               legend.key = element_rect(colour = NA),
               legend.position = "bottom",
               legend.direction = "horizontal",
               legend.key.size= unit(0.2, "cm"),
               legend.spacing = unit(0, "cm"),
               legend.title = element_text(face="italic"),
               legend.margin = margin(t = 0, unit = "cm"),
               plot.margin = unit(c(5,5,5,5),"mm"),
               strip.background=element_rect(colour="#f0f0f0",fill="#f0f0f0"),
               strip.text = element_text(face="bold")
          ))
}

scale_fill_Publication <- function(...){
      library(scales)
      discrete_scale("fill","Publication",manual_pal(values = c("#386cb0","#fdb462","#7fc97f","#ef3b2c","#662506","#a6cee3","#fb9a99","#984ea3","#ffff33")), ...)

}

scale_colour_Publication <- function(...){
      library(scales)
      discrete_scale("colour","Publication",manual_pal(values = c("#386cb0","#fdb462","#7fc97f","#ef3b2c","#662506","#a6cee3","#fb9a99","#984ea3","#ffff33")), ...)
}

make_cdf <- function(data, out_fname) {
   data <- melt(data)

   plt <- ggplot(data, aes(value, color = variable))
   plt <- plt + stat_ecdf(geom = "step", alpha = 0.75, size = 1.25) + coord_cartesian(xlim = c(0.0, 10.0), ylim = c(-0.01, 1.01), expand = 0)

   plt <- plt + theme_Publication() + scale_colour_Publication()

   plt <- plt + theme(legend.position = "right", legend.direction = "vertical", legend.key.size = unit(1, "cm"),
                  legend.title = element_text(face = "italic", size = 18),
                  legend.text = element_text(size = 18),
                  legend.background = element_rect(fill = alpha('white', 0.0)))

   plt <- plt + guides(colour = guide_legend(override.aes = list(size = 3, alpha = 1.0)))

   plt <- plt + labs(x = "Throughput (Gbps)", y = "CDF", color = "Rotation Speed:")

   ggsave(out_fname, plot = plt, width = 7, height = 3)
}

# data_2 <- read.table("2m_throughput.txt", header = TRUE, sep = "\t")

# colnames(data_2) <- c("Static", "0 mrad/s", "1 mrad/s", "5 mrad/s", "7 mrad/s")

# make_cdf(data_2, "graphs/2m_cdf.png")

# data_10 <- read.table("10m_throughput.txt", header = TRUE, sep = "\t")

# colnames(data_10) <- c("0 mrad/s", "1 mrad/s", "2.5 mrad/s", "4 mrad/s", "5 mrad/s", "6 mrad/s", "7 mrad/s")

# make_cdf(data_10, "graphs/10m_cdf.png")

# data_25 <- read.table("25m_throughput.txt", header = TRUE, sep = "\t")

# colnames(data_25) <- c("Static", "0 mrad/s", "0.5 mrad/s", "1 mrad/s")

# make_cdf(data_25, "graphs/25m_cdf.png")

data_new <- read.table("new_sfp_throughput.txt", header = TRUE, sep = "\t")
colnames(data_new) <- c("Static", "0 mrad/s", "1 mrad/s", "2.5 mrad/s", "5 mrad/s", "7 mrad/s")
make_cdf(data_new, "graphs/new_sfp_cdf.png")
