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

filter_data <- function(data){
	colnames(data) <- c("time", "Y", "Z", "X")

	return(subset(data, !is.na(Y)))
}

def_width <- 7
def_height <- 3

make_cdf <- function(data, f_name, scale_factor = 1.0, apply_abs = TRUE, legend_position = "right",  legend_direction = "vertical") {
	data <- melt(data[c("X", "Y", "Z")])

	data$value <- data$value * scale_factor

	if (apply_abs) {
		data$value <- abs(data$value)
	}

	plt <- ggplot(data, aes(value, color = variable)) + stat_ecdf(alpha = 0.75, size = 1.25)
	plt <- plt + theme_Publication() + scale_fill_Publication() + scale_colour_Publication(labels = c("X", "Y", "Z"))

	plt <- plt + theme(legend.position = legend_position, legend.direction = legend_direction, legend.key.size = unit(1, "cm"),
						legend.title = element_text(face = "italic", size = 18),
						legend.text = element_text(size = 18),
						legend.background = element_rect(fill = alpha('white', 0.0)))

	plt <- plt + guides(colour = guide_legend(override.aes = list(size = 3, alpha = 1.0)))

   plt <- plt + scale_x_continuous(breaks = round(seq(0, 7)))

   plt <- plt + labs(x = "Angular Speed (mrad/s)", y = "CDF", colour = "Axis of Rotation:")


	ggsave(paste(c("graphs/", f_name, ".png"), collapse = ""), plot = plt, width = def_width, height = def_height)
}

j_data <- read.table("data/All Data - J.tsv", header = TRUE, sep = "\t")
s10_data <- read.table("data/All Data - S10.tsv", header = TRUE, sep = "\t")
s60_data <- read.table("data/All Data - S60.tsv", header = TRUE, sep = "\t")
top1_data <- read.table("data/All Data - test1top.tsv", header = TRUE, sep = "\t")
top2_data <- read.table("data/All Data - test2top.tsv", header = TRUE, sep = "\t")
top3_data <- read.table("data/All Data - test3top.tsv", header = TRUE, sep = "\t")

j_data <- filter_data(j_data)
s10_data <- filter_data(s10_data)
s60_data <- filter_data(s60_data)
top1_data <- filter_data(top1_data)
top2_data <- filter_data(top2_data)
top3_data <- filter_data(top3_data)

all_data <- rbind(top1_data, top2_data, top3_data)

# make_cdf(j_data, "j_data", scale_factor = 1000.0, apply_abs = TRUE)
# make_cdf(s10_data, "s10_data", scale_factor = 1000.0, apply_abs = TRUE)
# make_cdf(s60_data, "s60_data", scale_factor = 1000.0, apply_abs = TRUE)
# make_cdf(top1_data, "top1_data", scale_factor = 1000.0, apply_abs = TRUE)
# make_cdf(top2_data, "top2_data", scale_factor = 1000.0, apply_abs = TRUE)
# make_cdf(top3_data, "top3_data", scale_factor = 1000.0, apply_abs = TRUE)

make_cdf(all_data, "angular_speed_cdf_sidelegend", scale_factor = 1000.0, apply_abs = TRUE)
